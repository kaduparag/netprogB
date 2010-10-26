/*
 * server.c
 *
 *  Created on: Oct 15, 2010
 *      Author: kaduparag
 */
//-----------------------------------------------------------------------------------
#include	"server.h"
//---------------------------GLOBAL----------------------------------------------------
int interfaceCount = 0;
int socketDescriptors[MAX_INTERFACE];//TODO Make MAX_INTERFACE dynamic by using getInterfaceInfo().
int server_port, max_win_size;
int TIMEOUT_SEC=1;
int TIMEOUT_USEC=0;
//----------------------------------------------------------------------------------

int mydg_echo(int ,const char *);

//-------------------------------------------------------------------------------
struct interface_info * getInterfaceInfo() {
	struct interface_info *ifi;
	ifi = calloc(1, sizeof(struct interface_info));
	if (ifi == NULL) {
		err_sys("calloc error");
	}
	char str[INET_ADDRSTRLEN];
	strcpy(ifi->ifi_name, "localhost");
	strcpy(str, "127.0.0.1");
	ifi->ifi_addr = calloc(1, sizeof(struct sockaddr));
	if (inet_pton(AF_INET, str, ifi->ifi_addr) != 1) {
		err_sys("Cannot convert string IP to binary IP.");
	}
	return ifi;
}
//--------------------------------------------------------------------------------
int main(int argc, char **argv) {
	int i;
	const int on = 1;
	pid_t pid;
	int child_pid;
	struct interface_info *ifi, *ifihead;
	struct sockaddr_in *sa;
	//I/O Multiplexing Select options
	int maxfdp1;
	fd_set rset;
	int maxSocketDescriptor = -999;
	char serverIP[INET_ADDRSTRLEN];

	printf("<start>\n");

	//	if (argc != 2)  //TODO Uncomment
	//			err_sys("usage: server <server.in file>");

	//Read input configuration from server.in
	char line[MAX_LINE];
	FILE* fp = fopen("server.in", "r"); //TODO read from argv[1]
	if (fgets(line, sizeof(line), fp)) {//Line 1 server port
		server_port = atoi(line);
		if (server_port == 0) {
			err_sys(
					"Invalid or missing server port number in the configuration file.");
		}
		printf("[INFO] Server port:%d\n", server_port);
	} else {
		err_sys("Invalid or missing input configuration.");
	}

	if (fgets(line, sizeof(line), fp)) {//Line 2 maximum sliding window size.
		max_win_size = atoi(line);
		if (max_win_size == 0) {
			err_sys(
					"Invalid or missing Max win size in the configuration file.");
		}
		printf("[INFO] Max win size port:%d\n", max_win_size);
	} else {
		err_sys("Invalid or missing input configuration.");
	}

	fclose(fp);

	for (ifihead = ifi = getInterfaceInfo(); ifi != NULL; ifi = ifi->ifi_next) {
		/*bind unicast address */
		socketDescriptors[interfaceCount] = socket(AF_INET, SOCK_DGRAM, 0);
		if (socketDescriptors[interfaceCount] < 0) {
			err_sys("socket error.");
		}

		if (setsockopt(socketDescriptors[interfaceCount], SOL_SOCKET,
				SO_REUSEADDR, &on, sizeof(on)))
			err_sys("Couldnt set Echo socket option");

		sa = (struct sockaddr_in *) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = htons(server_port);
		if (bind(socketDescriptors[interfaceCount], (struct sockaddr *) sa,	sizeof(*sa)))
			err_sys("Coudlnt bind socket.");

		printf("[INFO] Bounded to %s\n", Sock_ntop((struct sockaddr *) sa,sizeof(*sa)));

		interfaceCount++;
	}

	printf("[INFO] Total number of interfaces: %d\n", interfaceCount);

	//Using select monitor different sockets bounded to available unicast interfaces.
	for (;;) {
		//printf("[INFO] Parent server waiting for incoming requests...\n"); //TODO Uncomment info
		FD_ZERO(&rset);
		for (i = 0; i < interfaceCount; i++) {
			FD_SET(socketDescriptors[i], &rset);
			maxSocketDescriptor = max(maxSocketDescriptor,socketDescriptors[i]);
		}
		maxfdp1 = maxSocketDescriptor + 1;
		select(maxfdp1, &rset, NULL, NULL, NULL);
		for (i = 0; i < interfaceCount; i++) {
			if (FD_ISSET(socketDescriptors[i], &rset)) { // one of the socket descriptor is ready
				//fork a child
				if ((pid = fork()) == 0) { /* child */
					child_pid = mydg_echo(socketDescriptors[i],	inet_ntop(AF_INET, &sa->sin_addr, serverIP,
									sizeof(serverIP)));
					printf("[INFO] Child server %d closed.\n", child_pid);
					exit(0);
				} else if (pid == -1) {
					err_sys("Couldnt fork a child.");
				} else {
					//parent. DO nothing
				}
			}
		}
	}

	return 0;
}

//----------------------------------------------------------------------------------
int mydg_echo(int sockfd,const char * myaddr) {
	int n, i, connection_sockfd,success_flag,attempt_count;
	char filename[FILE_NAME_LEN], con_sock_port[8];//TODO con_sock_port size? FILE_NAME_LEN?
	char sendline[MAXLINE], recvline[MAXLINE + 1];
	socklen_t  addrlen,clilen;
	struct sockaddr_in localaddr,cliaddr;

	//Close all other socket descriptor
	for (i = 0; i < interfaceCount; i++) {
		if (socketDescriptors[i] != sockfd)
			close(sockfd);
	}

	//Read filename from client
	 clilen=sizeof(cliaddr);
	n = recvfrom(sockfd, filename, MAXLINE, 0, (struct sockaddr *) &cliaddr, &clilen);
	if (n < 0) {
		err_sys("Data receive error.");
	}
	filename[n] = 0; //null terminate
	printf("[INFO] Child server %d datagram from %s", getpid(), Sock_ntop(
			(struct sockaddr *) &cliaddr, clilen));
	printf(", to %s\n", myaddr);
	printf("[INFO] File requested %s\n", filename);

	//Start a new connection socket at ephemeral port.
	connection_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(connection_sockfd < 0){
		err_sys("Connection socket error.Cannot open the socket.");
	}
	bzero(&localaddr, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_port = htons(0);
	if (inet_pton(AF_INET, myaddr, &localaddr.sin_addr) != 1) //Using the same IP as the parent listening socket
		err_sys("Cannot convert string IP to binary IP.");

	if (bind(connection_sockfd, (struct sockaddr *) &localaddr,
			sizeof(localaddr)))
		err_sys("Coudlnt bind socket.");
//
//	printf("[INFO] Bounded to %s\n", Sock_ntop((struct sockaddr *) &localaddr,
//					sizeof(localaddr)));
	addrlen=sizeof(localaddr);
	//Get the ephemeral port  number.
	getsockname(connection_sockfd, (struct sockaddr *) &localaddr, &addrlen);
	sprintf(con_sock_port, "%d",  ntohs(localaddr.sin_port));

	//Print connection socket information
	printf("[INFO] Connection socket IP %s", myaddr);
	printf(", ephemeral port %s\n", con_sock_port);

	 // getchar();
	//Send the new connection socket ephemeral port.
	if ((n = sendto(sockfd, con_sock_port, strlen(con_sock_port), 0, (struct sockaddr *) &cliaddr,
			clilen)) != strlen(con_sock_port)) { //using the parent listening socket
		err_sys("Data send error.");
	}

	success_flag=0;
	for(attempt_count=0;attempt_count<MAX_ATTEMPT;attempt_count++){
       if(readable_timeo(connection_sockfd,TIMEOUT_SEC,TIMEOUT_USEC)==0){
          printf("Socket timeout...attempt %d failed.\n",attempt_count);
          //Send the new connection socket ephemeral port.
          	if ((n = sendto(sockfd, con_sock_port, strlen(con_sock_port), 0, (struct sockaddr *) &cliaddr,
          			clilen)) != strlen(con_sock_port)) { //using the parent listening socket
          		err_sys("Data send error.");
          	}
       }else{
    	   success_flag=1;
         break;
       }
	}
	 if(success_flag==0){//TODO what to do if MAX_ATTEMT fail? report and end prgm?
	        err_sys("Socket timeout...max no. of attempt failed.\n");
	 }

  //Wait for connected ack
	n = recvfrom(connection_sockfd, recvline, MAXLINE, 0, (struct sockaddr *) &cliaddr, &clilen);
	if (n < 0) {
		err_sys("Data receive error.");
	}
	recvline[n] = 0; //null terminate
    printf("%s\n",recvline);
    fflush(NULL);
	//Send the file on connection socket and close the parent listening socket
	close(sockfd);
	if (sendto(connection_sockfd, filename, strlen(filename), 0, (struct sockaddr *) &cliaddr, clilen)
			!= strlen(filename)) { //using the parent listening socket
		err_sys("Data send error.");
	}

	if (sendto(connection_sockfd, 0, 0, 0, (struct sockaddr *) &cliaddr, clilen) != 0) { //using the parent listening socket
		err_sys("Data send error.");
	}


	return getpid();
}
/* end mydg_echo */
