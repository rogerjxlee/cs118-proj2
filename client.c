#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>  
#include <stdlib.h>
#include <strings.h>

#include <string.h>
#include <time.h>

void error(char *msg)
{
    perror(msg);
    exit(0);
}

#define MAX_DATA_SIZE 1024
#define HEADER_SIZE 4*4
#define PACKET_SIZE MAX_DATA_SIZE + HEADER_SIZE

typedef struct  
{
	uint32_t seq;
	uint32_t ack;
	uint32_t fin;
	uint32_t length;
	char data[MAX_DATA_SIZE];
} packet;

int probability (float givenProb);

int main(int argc, char *argv[])
{
    int sockfd; //Socket descriptor
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address


    if (argc != 6) {
       fprintf(stderr,"Usage: %s <sender hostname> <sender portnumber> <filename> Pl PC\n", argv[0]);
       exit(0);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new UDP socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    float pl = (atof(argv[4])); //Probability of corrupt packet
    float pc = (atof(argv[5])); //Probability of lost packet
    
    /*********************************/
    /****** Start file transfer ******/

    // Request for file (send filename)
    packet *reqPacket;
    reqPacket->seq = 0;
    reqPacket->length = strlen(argv[3])+1;
	reqPacket->fin = 0;
	strncpy(reqPacket->data, argv[3], MAX_DATA_SIZE);

	//Print out Send request
	printf("DATA requested seq#%i, ack#%i, fin %i, content-length: %i", 
        	reqPacket->seq, reqPacket->ack, reqPacket->fin, reqPacket->length);
	
   if (sendto(sockfd, &reqPacket, sizeof(reqPacket), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
    	error("ERROR: sending failed"); 
    	return 0;
    }

    socklen_t serv_len = sizeof(serv_addr);
    
    long bytesRead = 0;
    char* newFile = malloc(sizeof(char)*(PACKET_SIZE));
    char* buffer = malloc(sizeof(char)*PACKET_SIZE);  
    long memSize = PACKET_SIZE;
    long seqCount = 0;
    int complete = 0;

    // Begin ACK response Sequence
    while (1)
    {
	    memset(buffer, 0, PACKET_SIZE);

	    int recvlen = recvfrom(sockfd, buffer, PACKET_SIZE, 0, (struct sockaddr *)&serv_addr, &serv_len);
        if (recvlen < 0)
            error("ERROR on receiving");

        packet* recvPacket = (packet*) buffer;

        srand(time(NULL));
        // Simulate corrupt or dropped packets
        int corrupt = probability(pc);
        int dropped = probability(pl);

        // Packet Lost or Dropped
        if (corrupt || dropped)
        {
            if (dropped)
            {
				printf("DATA received seq#%i, ack#%i, fin %i, content-length: %i  *DROPPED*", 
					recvPacket->seq, recvPacket->ack, recvPacket->fin, recvPacket->length);
                continue;
            }
            else if (corrupt)
                printf("DATA received seq#%i, ack#%i, fin %i, content-length: %i  *CORRUPTED*", 
					recvPacket->seq, recvPacket->ack, recvPacket->fin, recvPacket->length);

        }

        //File not found on server
        if(recvPacket->ack == 0)
        {
            fprintf(stderr, "Server encountered error\n");
            break;
        }

        if (!corrupt)
		  printf("DATA received seq#%i, ack#%i, fin %i, content-length: %i", 
        	recvPacket->seq, recvPacket->ack, recvPacket->fin, recvPacket->length);
		
        // ACK response Packet
        packet* ackPacket;
		ackPacket->seq = 0;
        ackPacket->length = 0;

        // Checks if the received packet is the expected sequence # or if corrupt
        if (recvPacket->seq != seqCount || corrupt) {
            ackPacket->ack = seqCount;
		}
        else
        {
            ackPacket->seq = recvPacket->ack; // Sequence # doesn't matter so just set the client seq to the server ack
			ackPacket->ack = recvPacket->seq + recvPacket->length; // ACK = received sequence # + length of data packet
            seqCount = ackPacket->seq + recvPacket->length; // Update the expected sequence #

            // realloc more memory as needed
            if (bytesRead + recvPacket->length > memSize)
            {
                memSize += PACKET_SIZE;
                newFile = realloc(newFile, sizeof(char)*memSize);
            }

            // Store data
            memcpy(newFile + bytesRead, recvPacket->data, recvPacket->length);
            bytesRead += recvPacket->length;

        }

	   // Send ACK with proper sequence number
	   if (sendto(sockfd, &ackPacket, HEADER_SIZE, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	   {
	    	error("ERROR: sending failed"); 
	    	return 0;
	   }
       printf("ACK sent: seq#%i, ack#%i, fin %i, content-length: %i", 
        	ackPacket->seq, ackPacket->ack, ackPacket->fin, ackPacket->length);

	   //File download complete. Write file to disk and break.
        if(recvPacket->fin == 1)
        {
            printf("File Transmission Complete, saving file as recv_%s\n", argv[3]);
            char* newFileName = malloc(sizeof(char)*(strlen(argv[3])) + 6);
            strcpy(newFileName, "recv_");
            strcat(newFileName, argv[3]);

            FILE* fp = fopen(newFileName, "w");
            fwrite(newFile, 1, bytesRead, fp);
            fclose(fp);
            break;
        }
	} // end while
	
    free(buffer);
    free(newFile);
    close(sockfd); //close socket
    
    return 0;
}

// Returns true with probability givenProb; 0 <= givenProb <= 1
int probability (float givenProb)
{
    if (givenProb <= 0)
    return 0;

    // Random floats from 0 to 1
    float randomProb = (float)rand()/(float)(RAND_MAX/1.0);
    return randomProb <= givenProb;
}
