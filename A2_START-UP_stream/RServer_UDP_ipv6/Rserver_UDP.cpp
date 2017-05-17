//159.334 - Networks
// SERVER: prototype for assignment 2.
//Note that this progam is not yet cross-platform-capable
// This code is different than the one used in previous semesters...
//************************************************************************/
//RUN WITH: Rserver_UDP 1235 0 0
//RUN WITH: Rserver_UDP 1235 1 0
//RUN WITH: Rserver_UDP 1235 0 1
//RUN WITH: Rserver_UDP 1235 1 1
//************************************************************************/

//---
#if defined __unix__ || defined __APPLE__
	#include <unistd.h>
	#include <errno.h>
	#include <stdlib.h>
	#include <stdio.h>
	#include <string.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <netdb.h>
  #include <iostream>
#elif defined _WIN32


	//Ws2_32.lib
	#define _WIN32_WINNT 0x501  //to recognise getaddrinfo()

	//"For historical reasons, the Windows.h header defaults to including the Winsock.h header file for Windows Sockets 1.1. The declarations in the Winsock.h header file will conflict with the declarations in the Winsock2.h header file required by Windows Sockets 2.0. The WIN32_LEAN_AND_MEAN macro prevents the Winsock.h from being included by the Windows.h header"
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <iostream>
	#include <vector>
#endif

#include "myrandomizer.h"

using namespace std;

#define BUFFER_SIZE 80  //used by receive_buffer and send_buffer
                        //the BUFFER_SIZE needs to be at least big enough to receive the packet
#define SEGMENT_SIZE 78
#define GENERATOR 0x8005 //0x8005, generator for polynomial division

const int ARG_COUNT=4;
int numOfPacketsDamaged=0;
int numOfPacketsLost=0;
int numOfPacketsUncorrupted=0;

int packets_damagedbit=0;
int packets_lostbit=0;

//*******************************************************************
//Function to save lines and discard the header
//*******************************************************************
//You are allowed to change this. You will need to alter the NUMBER_OF_WORDS_IN_THE_HEADER if you add a CRC
#define NUMBER_OF_WORDS_IN_THE_HEADER 3

struct tempData {
	char * data;
	int packetNumber;
};

void extractTokens(char *str, int &CRC, char *&command, int &packetNumber, char *&data){
	char * pch;
  int tokenCounter=0;

  while (1) {
	 if (tokenCounter ==0) {
      pch = strtok (str, " ,.-'\r\n'");
    } else if (tokenCounter == 3) {
			// NOTE: THE DATA MIGHT CONTAIN SPACES SO REMOVED FROM THE STRTOK OPTIONS
		 	pch = strtok (NULL, "'\r\n'");
	 } else  {
			pch = strtok (NULL, " ,.-'\r\n'");
	 }

	 if(pch == NULL) break;
    switch(tokenCounter){
      case 0:
				CRC = atoi(pch);
			  break;
      case 1:
				command = new char[strlen(pch)];
				strcpy(command, pch);
        break;
		case 2:
				packetNumber = atoi(pch);
		    break;
		case 3:
			data = new char[strlen(pch)];
			strcpy(data, pch);
			break;
    }
	 tokenCounter++;
  }
}

unsigned int CRCpolynomial(char *buffer){
	unsigned char i;
	unsigned int rem=0x0000;
    unsigned int bufsize=strlen(buffer);

	while(bufsize--!=0){
		for(i=0x80;i!=0;i/=2){
			if((rem&0x8000)!=0){
				rem=rem<<1;
				rem^=GENERATOR;
			} else{
	   	       rem=rem<<1;
		    }
	  		if((*buffer&i)!=0){
			   rem^=GENERATOR;
			}
		}
		buffer++;
	}
	rem=rem&0xffff;
	return rem;
}

#define WSVERS MAKEWORD(2,0)
WSADATA wsadata;

//*******************************************************************
//MAIN
//*******************************************************************
int main(int argc, char *argv[]) {
//********************************************************************
// INITIALIZATION
//********************************************************************
	struct sockaddr_storage clientAddress; //IPv4 & IPv6 -compliant
	struct addrinfo *result = NULL;
    struct addrinfo hints;
		int iResult;
    SOCKET s;
    char send_buffer[BUFFER_SIZE],receive_buffer[BUFFER_SIZE];
    int n,bytes,addrlen;

		memset(&hints, 0, sizeof(struct addrinfo));

		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_PASSIVE; // For wildcard IP address
    randominit();
//********************************************************************
// WSSTARTUP
//********************************************************************
   if (WSAStartup(WSVERS, &wsadata) != 0) {
      WSACleanup();
      printf("WSAStartup failed\n");
   }

	if (argc != ARG_COUNT) {
	   printf("USAGE: Rserver_UDP localport allow_corrupted_bits(0 or 1) allow_packet_loss(0 or 1)\n");
	   exit(1);
   }

	iResult = getaddrinfo(NULL, argv[1], &hints, &result); //converts human-readable text strings representing hostnames or IP addresses
	                                                        //into a dynamically allocated linked list of struct addrinfo structures
																			  //IPV4 & IPV6-compliant

	if (iResult != 0) {
    printf("getaddrinfo failed: %d\n", iResult);
    WSACleanup();
    return 1;
   }

//********************************************************************
//SOCKET
//********************************************************************
   s = INVALID_SOCKET; //socket for listening
	// Create a SOCKET for the server to listen for client connections

	s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	//check for errors in socket allocation
	if (s == INVALID_SOCKET) {
		 printf("Error at socket(): %d\n", WSAGetLastError());
		 freeaddrinfo(result);
		 WSACleanup();
		 exit(1);//return 1;
	}

   packets_damagedbit=atoi(argv[2]);
   packets_lostbit=atoi(argv[3]);
   if (packets_damagedbit < 0 || packets_damagedbit > 1 || packets_lostbit < 0 || packets_lostbit > 1){
	   printf("USAGE: Rserver_UDP localport allow_corrupted_bits(0 or 1) allow_packet_loss(0 or 1)\n");
	   exit(0);
   }
	//********************************************************************
	//BIND
	//********************************************************************
   iResult = bind( s, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);

        closesocket(s);
        WSACleanup();
        return 1;
    }
    cout << "==============<< UDP SERVER >>=============" << endl;
    cout << "channel can damage packets=" << packets_damagedbit << endl;
    cout << "channel can lose packets=" << packets_lostbit << endl;

	 freeaddrinfo(result); //free the memory allocated by the getaddrinfo
	                       //function for the server's address, as it is
	                       //no longer needed
//********************************************************************
//INFINITE LOOP
//********************************************************************
	// Variables
	 vector <tempData *> temp_buffer;
	 vector <char *> write_buffer;
	 int counter = 0;
	 FILE *fout = fopen("data_received.txt","w");
   while (1) {
		addrlen = sizeof(clientAddress); //IPv4 & IPv6-compliant
		memset(receive_buffer,0,sizeof(receive_buffer));
		bytes = recvfrom(s, receive_buffer, SEGMENT_SIZE, 0, (struct sockaddr*)&clientAddress, &addrlen);
		//IDENTIFY UDP client's IP address and port number.
		char clientHost[NI_MAXHOST];
    char clientService[NI_MAXSERV];
    memset(clientHost, 0, sizeof(clientHost));
    memset(clientService, 0, sizeof(clientService));
    getnameinfo((struct sockaddr *)&clientAddress, addrlen, clientHost, sizeof(clientHost), clientService, sizeof(clientService), NI_NUMERICHOST);
    printf("\nReceived a packet of size %d bytes from <<<UDP Client>>> with IP address:%s, at Port:%s\n",bytes,clientHost, clientService);
		//PROCESS RECEIVED PACKET
		//Remove trailing CR and LF
		n = 0;
		while(n < bytes){
			n++;
			if ((bytes < 0) || (bytes == 0)) break;
			if (receive_buffer[n] == '\n') { /*end on a LF*/
				receive_buffer[n] = '\0';
				break;
			}
			if (receive_buffer[n] == '\r') /*ignore CRs*/
				receive_buffer[n] = '\0';
		}
		if ((bytes < 0) || (bytes == 0)) break;
		printf("\n================================================\n");
		printf("RECEIVED --> %s \n",receive_buffer);
			if (strncmp(receive_buffer,"CLOSE",5)==0)  {//if client says "CLOSE", the last packet for the file was sent. Close the file
				// Remember that the packet carrying "CLOSE" may be lost or damaged as well!
				closesocket(s);
				printf("Server saved data_received.txt \n"); // you have to manually check to see if this file is identical to file1_Windows.txt
				printf("Closing the socket connection and Exiting...\n");
				break;
			}
			else {//it is not a PACKET nor a CLOSE; therefore, it might be a damaged packet
				   //Are you going to do nothing, ignoring the damaged packet?
				   //Or, send a negative ACK? It is up to you to decide here.
					 int CRC_recv = 0;
					 char *command;
					 int packet_number = 0;
					 char *data;
					 extractTokens(receive_buffer, CRC_recv, command, packet_number, data);
					 int CRC = CRCpolynomial(data);
					 // DEBUG:
					/* printf ("CRC RECIEVED: %d\n", CRC_recv);
					 printf("CRC FROM DATA: %d\n", CRC);
					 printf("COMMAND: %s\n",command);
					 printf("PACKET NUM: %d\n", packet_number);
					 printf("DATA: \"%s\"\n", data);*/
					 if (CRC == CRC_recv) {
						 if (packet_number == counter) {
							 write_buffer.push_back(data);
							 counter++;
							 sprintf(send_buffer,"ACK %d \r\n",packet_number);
	 	 		 			//send ACK ureliably
	 	 		 			send_unreliably(s,send_buffer,(sockaddr*)&clientAddress );
							// Check if any packets in temp_buffer can be added and counter moved up
								while (1) {
									if (temp_buffer.size() != 0 && temp_buffer.at(0)->packetNumber == counter) {
										write_buffer.push_back(temp_buffer.at(0)->data);
										counter ++;
										temp_buffer.erase(temp_buffer.begin());
									}
									else { break; }
								}
						 } else if (packet_number > counter && packet_number <= counter + 3) {
							 // Add value to temp buffer
							 tempData *temp = new tempData;
							 temp->packetNumber = packet_number;
							 temp->data = new char[strlen(data)];
							 temp->data = data;
							 temp_buffer.push_back(temp);
							 sprintf(send_buffer,"ACK %d \r\n",packet_number);
	 	 		 			//send ACK ureliably
	 	 		 			send_unreliably(s,send_buffer,(sockaddr*)&clientAddress );
						 } else if (packet_number < counter && packet_number >= counter - 4) {
							 // Send ACK to catch sender up to where we are and the discard contents of packet
							 sprintf(send_buffer,"ACK %d \r\n",packet_number);
	 	 		 			//send ACK ureliably
	 	 		 			send_unreliably(s,send_buffer,(sockaddr*)&clientAddress );
						 }
					} else {
						// DAMAGED PACKET
						sprintf(send_buffer,"NAK %d \r\n",packet_number);
					 //send ACK ureliably
					 send_unreliably(s,send_buffer,(sockaddr*)&clientAddress );
				 }
			}
   }
   closesocket(s);
	 //store the packet's data into a file
	 // TODO: write write_buffer into file
	 if (write_buffer.size() != 0) {
		 vector<char *>::iterator it;
		 for (it = write_buffer.begin(); it != write_buffer.end(); ++it) {
			 char * toWrite = new char[strlen(*it)];
			 toWrite = (*it);
			 fprintf(fout, "%s\n", toWrite);
			 printf("Wrote to file: \"%s\"\n", toWrite);
		}
	 }
	 fclose(fout);
   cout << "==============<< STATISTICS >>=============" << endl;
   cout << "numOfPacketsDamaged=" << numOfPacketsDamaged << endl;
   cout << "numOfPacketsLost=" << numOfPacketsLost << endl;
   cout << "numOfPacketsUncorrupted=" << numOfPacketsUncorrupted << endl;
   cout << "===========================================" << endl;

   exit(0);
}
