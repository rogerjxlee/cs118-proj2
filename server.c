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
#include <sys/stat.h>

#define MAX_DATA_SIZE 1024
#define HEADER_SIZE 4*4
#define PACKET_SIZE MAX_DATA_SIZE + HEADER_SIZE

#define TIMEOUT 2

typedef struct  
{
	uint32_t seq;
	uint32_t ack;
	uint32_t fin;
	uint32_t length;
	char data[MAX_DATA_SIZE];
} packet;

int lostorcorrupt (float p) {
	return (p >= ((float) rand()) / ((float) RAND_MAX));
}

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
	int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    struct sigaction sa;          // for signal SIGCHLD
    if (argc != 5) {
        fprintf(stderr,"Usage: server <portnumber> <CWnd> <Pl> <Pc>\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
    	error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // read in cwd, pl, and pc parameters
    int cwnd = atoi(argv[2]);
    float pl = atof(argv[3]);
    float pc = atof(argv[4]);

    if (bind(sockfd, (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)) < 0) 
            error("ERROR on binding");
     
    listen(sockfd,5);
     
    clilen = sizeof(cli_addr);
     
    /****** Kill Zombie Processes ******/
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    /*********************************/
    
    /********** Start Server **********/

    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

   	fd_set readfds;
   	FD_ZERO(&readfds);
   	int readysocks;

    int recvlen;
    char buf[PACKET_SIZE];
    packet* recvpacket;

    long filesize = 0;
    FILE* fp = NULL;

    srand(time(NULL));

    printf("waiting for file request\n");

    while (1) {
        recvlen = recvfrom(sockfd, buf, PACKET_SIZE, 0, (struct sockaddr *) &cli_addr, &clilen);
        
        //printf("%i\n",cli_addr.sin_addr.s_addr);
        //printf("%i\n",cli_addr.sin_port);

        if (recvlen < 0) 
            error("ERROR on receiving\n");

        recvpacket = (packet*) buf;
        
        printf("DATA received seq#%i, ACK#%i, FIN %i, content-length: %i\n", 
        	recvpacket->seq, recvpacket->ack, recvpacket->fin, recvpacket->length);

        printf("File requested is \"%s\"\n", recvpacket->data);

        if(recvpacket-> seq== 0 && recvpacket->ack == 0) {
        	fp = fopen(recvpacket->data,"rb");
        	if (fp == NULL) {
        		packet errorpacket;
        		errorpacket.seq = 0;
        		errorpacket.ack = 0;
                errorpacket.fin = 0;
                errorpacket.length = 0;
        		strcpy(errorpacket.data, "File not found");
        		sendto(sockfd, &errorpacket, HEADER_SIZE, 0, (struct sockaddr *)&cli_addr, clilen);
                fprintf(stderr, "File \"%s\" not found \n", recvpacket->data);
        		continue;
        	}

        	fseek(fp,0L,SEEK_END);
            filesize = ftell(fp);
            fseek(fp, 0L, SEEK_SET);
            char contents[filesize+1];
            int read = fread(contents,1,filesize,fp);
            contents[filesize] = 0;
            int remainingbytes = filesize % MAX_DATA_SIZE;
            int numpackets = filesize / MAX_DATA_SIZE + (remainingbytes > 0);
            packet packets[numpackets];

            int i;
            for (i = 0; i < numpackets; i++) {
            	if (i == numpackets-1 && remainingbytes != 0) {
            		packets[i].length = remainingbytes;
            	}
            	else {
            		packets[i].length = MAX_DATA_SIZE;
            	}
            	packets[i].seq = i * MAX_DATA_SIZE;            	
            	packets[i].ack = recvpacket->length; // consider null byte
				packets[i].fin = 0;
				memcpy(packets[i].data , contents + packets[i].seq, packets[i].length);
            }

            int cwndhead = 0;
			int cwndtail = cwnd;
            //printf("numpackets:%i\n",numpackets);
			for (i = cwndhead; i < cwndtail; i++) {
				if (i < numpackets) {
					sendto(sockfd, &(packets[i]), packets[i].length + HEADER_SIZE, 0, (struct sockaddr *)&cli_addr, clilen);
					printf("DATA sent seq#%i, ACK#%i, FIN %i, content-length: %i\n", 
	        			packets[i].seq, packets[i].ack, packets[i].fin, packets[i].length);
				}
			}

			// ACK handing, data transmission, window sliding
			while(1) {
				FD_SET(sockfd, &readfds);
				readysocks = select(sockfd+1, &readfds, NULL, NULL, &tv);

                //printf("readysocks: %i\n",readysocks);

				if (readysocks == -1) {
					error("Select sockets error\n");
				}

				else if(readysocks) {
					recvlen = recvfrom(sockfd, buf, PACKET_SIZE, 0, (struct sockaddr *) &cli_addr, &clilen);

					if (recvlen < 0) 
	            		error("ERROR on receiving\n");

	        		recvpacket = (packet*) buf;

					

					int lost = lostorcorrupt(pl);
				    int corrupt = lostorcorrupt(pc);

				    if (lost || corrupt) {
				    	printf("(ACK lost or corrupted) Timeout\n");
				    	continue;
				    }

				    printf("ACK received seq#%i, ACK#%i, FIN %i, content-length: %i\n",
				    	recvpacket->seq, recvpacket->ack, recvpacket->fin, recvpacket->length);
				    int lastsent = cwndtail;

				    //if(!(recvpacket->ack == 0 && recvpacket->seq == 0 && recvpacket->length != 0)) {
				   	    
				   	    lastsent = cwndtail;
                        int requestedpacket = recvpacket->ack / MAX_DATA_SIZE + (recvpacket->ack % MAX_DATA_SIZE > 0);
                        //printf("cwndhead: %i\n", cwndhead);
                        //printf("ackpack/size: %i\n", requestedpacket);

				   	    if (requestedpacket > cwndhead) {
                            
    				   		cwndhead = requestedpacket;
				    	    cwndtail = cwndhead + cwnd;	
                            printf("sliding window\n");
				   	    }
				    //}
				    
				    for (i = lastsent; i < cwndtail; i++) {
				    	if (i < numpackets) {
				    		sendto(sockfd, &packets[i], packets[i].length + HEADER_SIZE, 0, (struct sockaddr *)&cli_addr, clilen);
				    		printf("DATA sent seq#%i, ACK#%i, FIN %i, content-length: %i\n", 
	        					packets[i].seq, packets[i].ack, packets[i].fin, packets[i].length);
						}
				    }
					if(cwndhead >= numpackets) {
						printf("file transfer complete\n");
						break;
					}

				}
				else {
					printf("timeout, retransmitting packets in window\n");
					for (i = cwndhead; i < cwndtail; i++) {
						if (i < numpackets) {
                            //printf("packet %i\n", i);
							sendto(sockfd, &packets[i], packets[i].length + HEADER_SIZE, 0, (struct sockaddr *)&cli_addr, clilen);
							printf("DATA sent seq#%i, ACK#%i, FIN %i, content-length: %i\n", 
			        			packets[i].seq, packets[i].ack, packets[i].fin, packets[i].length);
						}
					}
					tv.tv_sec = TIMEOUT;
				}
                tv.tv_sec = TIMEOUT;
			}

        	tv.tv_sec = TIMEOUT;
        	cwndhead = 0;
        	cwndtail = cwnd;
		    packet finpacket;
        	//Fin and Finack
            printf("File size: %i\n", (int)filesize);
            if(!(recvpacket->seq == 0 && recvpacket->ack == 0 && recvpacket->length != 0) && recvpacket->ack == filesize) {
        		finpacket.seq = recvpacket->ack;
        		finpacket.ack = recvpacket->seq + 1;
        		finpacket.fin = 1;
        		finpacket.length = 0;
        		sendto(sockfd, &finpacket, finpacket.length + HEADER_SIZE, 0, (struct sockaddr *)&cli_addr, clilen);
        		printf("DATA sent seq#%i, ACK#%i, FIN %i, content-length: %i\n", 
			        			finpacket.seq, finpacket.ack, finpacket.fin, finpacket.length);
            }

        	while(1) {
        		FD_SET(sockfd, &readfds);
				readysocks = select(sockfd+1, &readfds, NULL, NULL, &tv);

				if (readysocks == -1) {
					error("Select sockets error\n");
				}
				else if(readysocks) {
					recvlen = recvfrom(sockfd, buf, PACKET_SIZE, 0, (struct sockaddr *) &cli_addr, &clilen);

					if (recvlen < 0) 
	            		error("ERROR on receiving");

	        		recvpacket = (packet*) buf;

					int lost = lostorcorrupt(pl);
				    int corrupt = lostorcorrupt(pc);

				    if (lost || corrupt) {
				    	printf("(ACK lost or corrupted) Timeout\n");
				    	continue;
				    }

				    printf("FINACK received seq#%i, ACK#%i, FIN %i, content-length: %i\n",
				    	recvpacket->seq, recvpacket->ack, recvpacket->fin, recvpacket->length);

				    packet finackpacket;
		        	finackpacket.seq = recvpacket->ack;
		        	finackpacket.ack = recvpacket->seq + 1;
		        	finackpacket.fin = 1;
		        	finackpacket.length = 0;
		        	sendto(sockfd, &finackpacket, finackpacket.length + HEADER_SIZE, 0, (struct sockaddr *)&cli_addr, clilen);
		        	printf("FINACK sent seq#%i, ACK#%i, FIN %i, content-length: %i\n", 
                        finackpacket.seq, finackpacket.ack, finackpacket.fin, finackpacket.length);
                    printf("close connection\n");
				    break;
				}
				else {
					printf("timeout, retransmitting fin\n");
					sendto(sockfd, &finpacket, finpacket.length + HEADER_SIZE, 0, (struct sockaddr *)&cli_addr, clilen);
                    printf("DATA sent seq#%i, ACK#%i, FIN %i, content-length: %i\n", 
                        finpacket.seq, finpacket.ack, finpacket.fin, finpacket.length);
					tv.tv_sec = TIMEOUT;
				}
                tv.tv_sec = TIMEOUT;
        	}
        }

        pid = fork(); //create a new process
        if (pid < 0)
            error("ERROR on fork");
        
        if (pid == 0)  { // fork() returns a value of 0 to the child process
            close(sockfd);
            //dostuff(newsockfd);
            exit(0);
        }
        else //returns the process ID of the child process to the parent
            close(newsockfd); // parent doesn't need this 
    } /* end of while */
    return 0; /* we never get here */
}
