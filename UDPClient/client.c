/*
 * client.c
 *
 *  Created on: Oct 15, 2010
 *      Author: kaduparag
 */

#include	"client.h"

//Global
char serverIP[INET_ADDRSTRLEN];
int server_port, max_win_size;
int TIMEOUT_SEC=1;
int TIMEOUT_USEC=0;
char required_file_name[FILE_NAME_LEN];

void dg_cli(int sockfd, const struct sockaddr *pservaddr, socklen_t servlen);

//--------------------------------------------------------------------------------
void err_sys(const char * msg) {
	printf("ERROR: %s\n", msg);
	exit(1);
}
//--------------------------------------------------------------------------------

int isIPAddress(const char *addr) {
	int status = 1, i;
	int len = strlen(addr);
	for (i = 0; i < len; i++) {
		if (addr[i] == '.' || (addr[i] >= '0' && addr[i] <= '9')) {
			continue;
		} else {
			status = 0;
			break;
		}

	}
	return status;
}
//------------------------------------------------------------------------------
int main(int argc, char **argv) {
	int sockfd;
	struct sockaddr_in servaddr;
	int len;

	//	if (argc != 2) //TODO Uncomment
	//		err_sys("usage: client <client.in file>");

	//Read input configuration from server.in
	char line[MAX_LINE];
	FILE* fp = fopen("client.in", "r"); //TODO read from argv[1]
	if (fgets(line, sizeof(line), fp)) {//Line 1 server ip
		len = strlen(line) - 1; //remove the trailing \n
		if (line[len] == '\n')
			line[len] = '\0';
		strcpy(serverIP, line);
		if (!isIPAddress(serverIP)) {
			err_sys("Invalid or missing server ip in the configuration file.");
		}
		printf("[INFO] Server ip:%s\n", serverIP);
	} else {
		err_sys("Invalid or missing input configuration.");
	}

	if (fgets(line, sizeof(line), fp)) {//Line 2 server port
		server_port = atoi(line);
		if (server_port == 0) {
			err_sys(
					"Invalid or missing server port number in the configuration file.");
		}
		printf("[INFO] Server port:%d\n", server_port);
	} else {
		err_sys("Invalid or missing input configuration.");
	}

	if (fgets(line, sizeof(line), fp)) {//Line 3 file name
		len = strlen(line) - 1; //remove the trailing \n
		if (line[len] == '\n')
			line[len] = '\0';
		strcpy(required_file_name, line);
		if (len == 0)
			err_sys("Invalid or missing file name in the configuration file.");
		printf("[INFO] File name:%s\n", required_file_name);
	} else {
		err_sys("Invalid or missing input configuration.");
	}

	if (fgets(line, sizeof(line), fp)) {//Line 4 maximum sliding window size.
		max_win_size = atoi(line);
		if (max_win_size == 0) {
			err_sys(
					"Invalid or missing Max win size in the configuration file.");
		}
		printf("[INFO] Max win size:%d\n", max_win_size);
	} else {
		err_sys("Invalid or missing input configuration.");
	}

	fclose(fp);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(server_port);
	if (inet_pton(AF_INET, serverIP, &servaddr.sin_addr) != 1)
		err_sys("Cannot convert string IP to binary IP.");

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		err_sys("Socket error.");
	}

	dg_cli(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

	exit(0);
}

void dg_cli(int sockfd, const struct sockaddr *pservaddr, socklen_t servlen) {
	int n,connection_sockfd,attempt_count,success_flag;
	char sendline[MAXLINE], recvline[MAXLINE + 1];
	struct sockaddr_in servaddr,cliaddr;

	//Bind and print client socket addr.
	cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_port = htons(0);
	if (bind(sockfd, (struct sockaddr *) &cliaddr,sizeof(cliaddr)))
		err_sys("Coudlnt bind socket.");

    printf("[INFO] Bounded to %s\n", Sock_ntop((struct sockaddr * ) &cliaddr,sizeof(cliaddr)));

	//Connect to well known port of the server
	if (connect(sockfd, (struct sockaddr *) pservaddr, servlen) < 0) {
		err_sys("Connect error");
	}

	//Send the filename as datagram
	int len = strlen(required_file_name);
	if (write(sockfd, required_file_name, len) != len) {
		err_sys("Write error.Server is unreachable");
	}

	success_flag=0;
	for(attempt_count=0;attempt_count<MAX_ATTEMPT;attempt_count++){
       if(readable_timeo(sockfd,TIMEOUT_SEC,TIMEOUT_USEC)==0){
          printf("Socket timeout...attempt %d failed.\n",attempt_count);
          if (write(sockfd, required_file_name, len) != len) { //Try again
          		err_sys("Write error.Server is unreachable");
          	}
       }else{
    	   success_flag=1;
         break;
       }
	}
	 if(success_flag==0){//TODO what to do if MAX_ATTEMT fail? report and end prgm?
	        err_sys("Socket timeout...max no. of attempt failed.\n");
	 }

	//Read the new connection socket ephemeral port number. This serves as ack for filename too.
	if ((n = read(sockfd, recvline, MAXLINE)) == -1)
		err_sys("Read error. Server is unreachable");

	recvline[n] = 0; // null terminate
	fputs(recvline, stdout);

	//Close the old connection and connect to new one.
	close(sockfd);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(recvline));
	if (inet_pton(AF_INET, serverIP, &servaddr.sin_addr) != 1)
		err_sys("Cannot convert string IP to binary IP.");

	connection_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (connection_sockfd < 0) {
		err_sys("Socket error.");
	}
	if (connect(connection_sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		err_sys("Connect error");
	}

	//Send connected ack
	strcpy(sendline, "ACK_CONNECTED");
	if (write(connection_sockfd, sendline, strlen(sendline)) != strlen(sendline)) {
		err_sys("Write error.Server is unreachable");
	}
	printf("%s\n",sendline);
	//	fflush(NULL);
	//Read the file and print output.
	while (1) {
		if ((n = read(connection_sockfd, recvline, MAXLINE)) == -1)
			err_sys("Read error. Server is unreachable");
		else if (n == 0)
			break;

		recvline[n] = 0; // null terminate
		fputs(recvline, stdout);
		//send ACK
		/*int len = strlen(sendline);
		if (write(connection_sockfd, sendline, len) != len) {
			err_sys("Write error.Server is unreachable");
		}*/
	}
	printf("[INFO] File transfer completed.\n");
}
