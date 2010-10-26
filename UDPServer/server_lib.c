/*
 * server_lib.c
 *
 *  Created on: Oct 15, 2010
 *      Author: kaduparag
 */
#include	"server.h"

int readable_timeo(int fd, int sec,int usec) {
	fd_set rset;
	struct timeval tv;

	FD_ZERO(&rset);
	FD_SET(fd, &rset);

	tv.tv_sec = sec;
	tv.tv_usec = usec;

	return (select(fd + 1, &rset, NULL, NULL, &tv));
	/* 4> 0 if descriptor is readable */
}

//--------------------------------------------------------------------------------
void err_sys(const char * msg) {
	printf("[ERROR] %s\n", msg);
	exit(1);
}
//------------------------------------------------------------------------------
char *
sock_ntop(const struct sockaddr *sa, socklen_t salen) {
	char portstr[8];
	static char str[128]; /* Unix domain is largest */

	switch (sa->sa_family) {
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *) sa;

		if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
			return (NULL);
		if (ntohs(sin->sin_port) != 0) {
			snprintf(portstr, sizeof(portstr), ":%d", ntohs(sin->sin_port));
			strcat(str, portstr);
		}
		return (str);
	}
		/* end sock_ntop */

	default:
		snprintf(str, sizeof(str), "sock_ntop: unknown AF_xxx: %d, len %d",
				sa->sa_family, salen);
		return (str);
	}
	return (NULL);
}

char *
Sock_ntop(const struct sockaddr *sa, socklen_t salen) {
	char *ptr;

	if ((ptr = sock_ntop(sa, salen)) == NULL)
		err_sys("sock_ntop error"); /* inet_ntop() sets errno */
	return (ptr);
}



int isIPAddress(const char *addr) {
   int status=1,i;
   int len=strlen(addr);
   for(i=0;i<len;i++){
      if(addr[i]=='.' || (addr[i] >= '0' && addr[i]  <= '9')){
        continue;
      }else{
    	  status=0;
    	  break;
      }

   }
   return status;
}
