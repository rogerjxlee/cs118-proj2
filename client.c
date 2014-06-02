#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <string.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#define PACKET_SZIE 1000
#define HEADER_SIZE 4*3

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
	int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    struct sigaction sa;          // for signal SIGCHLD
    if (argc != 6) {
        fprintf(stderr,"Usage: client <sender_hostname> <sender_portnumber> <filename> <Pl> <Pc>\n");
        exit(1);
    }

    sockfd = socket(AF_INET. SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     
}