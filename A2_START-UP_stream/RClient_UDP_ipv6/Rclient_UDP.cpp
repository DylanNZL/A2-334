//159.334 - Networks
// CLIENT: prototype for assignment 2.
//Note that this progam is not yet cross-platform-capable
// This code is different than the one used in previous semesters...
//************************************************************************/
//RUN WITH: Rclient_UDP 127.0.0.1 1235 0 0
//RUN WITH: Rclient_UDP 127.0.0.1 1235 0 1
//RUN WITH: Rclient_UDP 127.0.0.1 1235 1 0
//RUN WITH: Rclient_UDP 127.0.0.1 1235 1 1
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
	#include <conio.h>
	#include <windows.h>
	#include <ctime>
#endif

#include "myrandomizer.h"

using namespace std;

#define GENERATOR 0x8005 //0x8005, generator for polynomial division
#define WSVERS MAKEWORD(2,0)
#define BUFFER_SIZE 80  //used by receive_buffer and send_buffer
                        //the BUFFER_SIZE has to be at least big enough to receive the packet
#define SEGMENT_SIZE 78
//segment size, i.e., if fgets gets more than this number of bytes it segments the message into smaller parts.

struct packet {
	packet(char* mPacket) {
		acknowledged = false;
		sentOnce = false;
		packet_data = mPacket;
		resendTime = clock(); // TODO: SET TO CURRENT TIME SO THAT IT WILL NEED TO BE SENT?
	}
	packet() {
	}
	bool acknowledged;
	bool sentOnce;
	char* packet_data;
	clock_t resendTime;
};

WSADATA wsadata;
const int ARG_COUNT=5;
//---
int numOfPacketsDamaged=0;
int numOfPacketsLost=0;
int numOfPacketsUncorrupted=0;

int packets_damagedbit=0;
int packets_lostbit=0;

// Used to define the number
unsigned int CRCpolynomial(char *buffer){
	unsigned char i;
	unsigned int rem=0x0000;
  unsigned int bufsize=strlen(buffer);

	while(bufsize--!=0){
		for(i = 0x80; i != 0; i /= 2){
			if((rem&0x8000) != 0){
				rem = rem << 1;
				rem ^= GENERATOR;
			} else{
	   	       rem = rem << 1;
		    }
	  		if((*buffer&i) != 0){
			   rem ^= GENERATOR;
			}
		}
		buffer++;
	}
	rem = rem&0xffff;
	return rem;
}


/////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
//*******************************************************************
// Initialization
//*******************************************************************
	struct sockaddr_storage localaddr, remoteaddr;
  char portNum[NI_MAXSERV];
  struct addrinfo *result = NULL;
  struct addrinfo hints;

  memset(&localaddr, 0, sizeof(localaddr));  //clean up
  memset(&remoteaddr, 0, sizeof(remoteaddr));//clean up
  randominit();
 	SOCKET s;
  char send_buffer[BUFFER_SIZE],receive_buffer[BUFFER_SIZE];
  int n,bytes,addrlen;

	addrlen=sizeof(struct sockaddr);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	//********************************************************************
	// WSSTARTUP
	//********************************************************************
	if (WSAStartup(WSVERS, &wsadata) != 0) {
    WSACleanup();
	  printf("WSAStartup failed\n");
	}
	//*******************************************************************
	//	Dealing with user's arguments
	//*******************************************************************
  if (argc != ARG_COUNT) {
		printf("USAGE: Rclient_UDP remote_IP-address remoteport allow_corrupted_bits(0 or 1) allow_packet_loss(0 or 1)\n");
	  exit(1);
  }
	int iResult = 0;
	sprintf(portNum,"%s", argv[2]);
	iResult = getaddrinfo(argv[1], portNum, &hints, &result);
  packets_damagedbit=atoi(argv[3]);
  packets_lostbit=atoi(argv[4]);
  if (packets_damagedbit < 0 || packets_damagedbit > 1 || packets_lostbit< 0 || packets_lostbit>1){
	  printf("USAGE: Rclient_UDP remote_IP-address remoteport allow_corrupted_bits(0 or 1) allow_packet_loss(0 or 1)\n");
	  exit(0);
  }
	//*******************************************************************
	//CREATE CLIENT'S SOCKET
	//*******************************************************************
	s = INVALID_SOCKET;
	s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (s == INVALID_SOCKET) {
	 printf("socket failed\n");
	 exit(1);
	}
  // nonblocking option
	// Set the socket I/O mode: In this case FIONBIO
	// enables or disables the blocking mode for the
	// socket based on the numerical value of iMode.
	// If iMode = 0, blocking is enabled;
	// If iMode != 0, non-blocking mode is enabled.
	u_long iMode=1;

  iResult=ioctlsocket(s,FIONBIO,&iMode);
  if (iResult != NO_ERROR){
 		printf("ioctlsocket failed with error: %d\n", iResult);
 		closesocket(s);
 	 	WSACleanup();
 	 	exit(0);
  }
  cout << "==============<< UDP CLIENT >>=============" << endl;
  cout << "channel can damage packets=" << packets_damagedbit << endl;
  cout << "channel can lose packets=" << packets_lostbit << endl;
	//*******************************************************************
	//SEND A TEXT FILE
	//*******************************************************************
	int counter = 0;
	vector<packet> packets; // CONTAINER FOR PACKETS
	packet closePacket = packet((char *) "CLOSE \r\n");
	int closeCount = 0;
	char temp_buffer[BUFFER_SIZE];
	FILE *fin=fopen("data_for_transmission.txt","rb"); //original
	if(fin == NULL) {
		printf("cannot open data_for_transmission.txt\n");
		closesocket(s);
		WSACleanup();
		exit(0);
	} else {
		printf("data_for_transmission.txt is now open for sending\n");
  }

	// READ IN FILE AND STORE AS PACKETS IN THE VECTOR
	while(1) {
		if (feof(fin)) { fclose(fin); break; }

		fgets(temp_buffer, SEGMENT_SIZE, fin); // get one line of data from the file
		strtok(temp_buffer, "'\r\n'"); // Remove /r/n
		unsigned int CRC = CRCpolynomial(temp_buffer); // CRC
		char *temp_packet = (char *) malloc(BUFFER_SIZE + 30);
		sprintf(temp_packet, "%d Packet %d %s\r\n", CRC, counter, temp_buffer); // Join into a packet string
		// DEBUG:
		/*printf("CRC: %d\n", CRC);
		printf("PACKET: %d\n", counter);
		printf("COMMAND: Packet\n");
		printf("DATA: \"%s\"\n", temp_buffer);
		printf("PACKET: %s\n", temp_packet);*/

		packet p = packet(temp_packet); // create a full packet struct
		packets.push_back(p);
		counter++;
	}

	int numPackets = counter - 1; // Take away one because it always increments the counter for the next packet.
	int windowBase = 0;

	while (1) {
		memset(send_buffer, 0, sizeof(send_buffer));//clean up the send_buffer before reading the next line

		/**
		 * SEND
		 */
		// All packets are sent so send close and wait for ack to close socket
		if (windowBase > numPackets) {
			if (!closePacket.sentOnce) {
				// DEBUG:
				cout <<"SENDING CLOSE\n";
				closePacket.sentOnce = true;
				sprintf(send_buffer, closePacket.packet_data); //send a CLOSE command to the RECEIVER (Server)
				send_unreliably(s,send_buffer,(result->ai_addr));
				closeCount++;
			} else if ((clock() - closePacket.resendTime) * 1000 / CLOCKS_PER_SEC > 500) {
				cout << "RESENDING CLOSE\n";
				sprintf(send_buffer, closePacket.packet_data); //send a CLOSE command to the RECEIVER (Server)
				send_unreliably(s,send_buffer,(result->ai_addr));
				closeCount++;
			}
			// If we've sent it more than 5 times give up
			// NOTE could be more or less as we may miss the first ack and just delay closing
			if (closeCount == 5) {
				break;
			}
		}
		// SEND/RESEND Data Packets
		else {
			for (int i = windowBase; i < windowBase + 3; i++) {
				if (i > numPackets) { break; }
				// If it hasn't been sent the first time and hasn't been acknowledged
				if (packets.at(i).sentOnce == false && packets.at(i).acknowledged == false) {
					cout << "Sending window base = " << windowBase << endl;
						//cout << "1) i is currently = " << i << endl;
						strcpy(send_buffer, packets.at(i).packet_data);
						// DEBUG:
						cout << "Sending: " << send_buffer << endl;
						//cout << "calling send_unreliably, to deliver data of size " << strlen(send_buffer) << endl;
						send_unreliably(s, send_buffer, (result->ai_addr)); //send the packet to the unreliable data channel
						packets.at(i).sentOnce = true;
						packets.at(i).resendTime = clock(); // TODO: IS THIS RIGHT?
					} else {
						// IF THE STORED CLOCK - THE CURRENT CLOCK DIVIDED BY CLOCKS_PER_SEC IS MORE THAN 1 THEN 1 SEC HAS PASSED
						double timeElapsed = (clock() - packets.at(i).resendTime) * 1000 / CLOCKS_PER_SEC;
						if ( timeElapsed > 500 ) {
							cout << "Sending window base = " << windowBase << endl;
							printf("%f\n\n", timeElapsed);
							strcpy(send_buffer, packets.at(i).packet_data);
							// DEBUG:
							cout << "Resending: " << send_buffer;
							//cout << "calling send_unreliably, to deliver data of size " << strlen(send_buffer) << endl;
							send_unreliably(s, send_buffer, (result->ai_addr)); //send the packet to the unreliable data channel
							packets.at(i).resendTime = clock();
						}
					}
				}
		}
		/**
		 * RECIEVE
		 */
		// NOTE Don't think it is needed anymore
		//Sleep(1);  // sleep for 1 millisecond
		addrlen = sizeof(remoteaddr); //IPv4 & IPv6-compliant
		bytes = recvfrom(s, receive_buffer, 78, 0,(struct sockaddr*)&remoteaddr,&addrlen);
		// IDENTIFY server's IP address and port number.
		char serverHost[NI_MAXHOST]; char serverService[NI_MAXSERV];
		memset(serverHost, 0, sizeof(serverHost)); memset(serverService, 0, sizeof(serverService));
		getnameinfo((struct sockaddr *)&remoteaddr, addrlen, serverHost, sizeof(serverHost), serverService, sizeof(serverService), NI_NUMERICHOST);
		//Remove trailing CR and LN
		if (bytes != SOCKET_ERROR || bytes != -1) {
			printf("\nReceived a packet of size %d bytes from <<<UDP Server>>> with IP address:%s, at Port:%s\n",bytes,serverHost, serverService);
			n = 0;
			while (n<bytes){
				n++;
				if ((bytes < 0) || (bytes == 0)) break;
				if (receive_buffer[n] == '\n') { /*end on a LF*/
					receive_buffer[n] = '\0';
					break;
				}
				if (receive_buffer[n] == '\r') /*ignore CRs*/
				receive_buffer[n] = '\0';
			}
			printf("RECEIVED --> %s, %d elements\n",receive_buffer, int(strlen(receive_buffer)));
			// RECIEVED ACK
			if (strncmp(receive_buffer, "ACK", 3) == 0) {
				strncpy(temp_buffer, &receive_buffer[4], 10);
				int packetRecieved = atoi(temp_buffer);
				printf("Recieved acknowledgement for packet %d\n", packetRecieved);
				if (packets.at(packetRecieved).sentOnce == true) {
					packets.at(packetRecieved).acknowledged = true;
				}
				// MOVE WINDOW
				if (windowBase == packetRecieved) {
					if (packetRecieved != numPackets) {
						int p = 1;
						while (1) {
							//cout << "2) p is currently = " << p << " with windowBase = " << windowBase << endl;
							if (windowBase + p >= numPackets && packets.at(numPackets).acknowledged != true) { windowBase = numPackets; break; }
							else if (windowBase + p <= numPackets && packets.at(windowBase + p).acknowledged != true) { windowBase += p; break; }
							else if (p >= 3) { windowBase += 4; break; }
							p++;
						}
					}
					else {
						windowBase++;
					}
				} else if (windowBase != packetRecieved) {
					  //cout << "packetRecieved= " << packetRecieved << endl;
						packets.at(packetRecieved).acknowledged = true;
				}
			} else if (strncmp(receive_buffer, "NAK", 3) == 0) {
				strncpy(temp_buffer, &receive_buffer[4], 10);
				try {
					int packetRecieved = atoi(temp_buffer);
					printf("Recieved NAK for packet %s\n", temp_buffer);
					// RESEND if it is inside the current window and not higher than the total packets to send
					if (packetRecieved <= numPackets && packetRecieved >= windowBase) {
						strcpy(send_buffer, packets.at(packetRecieved).packet_data);
						// DEBUG:
						cout << "Sending: " << send_buffer << endl;
						//cout << "calling send_unreliably, to deliver data of size " << strlen(send_buffer) << endl;
						send_unreliably(s, send_buffer, (result->ai_addr)); //send the packet to the unreliable data channel
						packets.at(packetRecieved).resendTime = clock();
					}
				} catch (...) {
					cout << "ERROR Packet " << temp_buffer << " is out of range." << endl;
				}
			} else if (strncmp(receive_buffer, "CLOSE ACK", 9) == 0) {
				cout << "RECIEVED CLOSE" << endl;
				closePacket.acknowledged = true;
				printf("End-of-File reached. \n");
				printf("\n======================================================\n");
				break;
			}
		}
	} //while loop
	//*******************************************************************
	//CLOSESOCKET
	//*******************************************************************
	closesocket(s);
  printf("Closing the socket connection and Exiting...\n");
  cout << "==============<< STATISTICS >>=============" << endl;
  cout << "numOfPacketsDamaged=" << numOfPacketsDamaged << endl;
  cout << "numOfPacketsLost=" << numOfPacketsLost << endl;
  cout << "numOfPacketsUncorrupted=" << numOfPacketsUncorrupted << endl;
  cout << "===========================================" << endl;
	exit(0);
}
