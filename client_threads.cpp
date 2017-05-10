#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>
#include <ctime>
#include <vector>

#define semaphore HANDLE
#define frame_size 4 //size of frame for Selective Repeat
#define sleepTime 1000 //RTT
#define timeOut 1200 //Wait time before resending packet

using namespace std;

void wait(semaphore h) {
	WaitForSingleObject(h, MAXLONG);
}

void signal(semaphore h) {
	ReleaseSemaphore(h,1,NULL);
}

semaphore create(int val) {
	return CreateSemaphore(NULL,(long)val, MAXLONG, NULL);
}

clock_t get_end_time() {
	clock_t end_time;
	end_time = clock() + 2 * CLOCKS_PER_SEC;
	return end_time;
}

struct packet {
	bool acknowledged;
	char* packet_data;
	clock_t end_time;
};

vector<packet> packets;
unsigned long frame_end = 0;
unsigned long number_of_packets;

semaphore can_send;



unsigned long CALLBACK send_packets(void* n) {
	while(frame_end < packets.size()) { //Need to change, while position < number_of_packets
		wait(can_send);
		cout << "sending packet: " << frame_end << endl; //
		packets[frame_end].end_time = get_end_time();
		++frame_end;
	}
	return 0;
}

unsigned long CALLBACK recieve_and_acknolwedge(void* n) {
	Sleep(20);
	unsigned long recieved = 0;
	int move_frame = 1;
	while(1) {
		if(recieved <= (frame_end - 4)) { //Acknoledgement of packet in the range of the frame //!!!!!!!!!!!!!!!!!!!!!!!!Add front of frame check for security
			if(recieved == (frame_end - 4)) { //Acknoledgment of the first packet in the frame, check if the next packet is already acknowledged 
				packets[recieved].acknowledged = true;
				while(packets[recieved+1].acknowledged == true) {
					++move_frame;
				}
			} else {
				packets[recieved].acknowledged = true;
			}
			for(int j = 0; j < move_frame; ++j) {
				signal(can_send);
			}
		} else {
			//Do nothing the packet is not in the frame range
		}
		cout << "Recvied acknowledgement" << endl;
		Sleep(10);
	}
	return 0;
}



int main() {
	cout << "stop" << endl;
	
	can_send = create(frame_size);
	
	for(int i = 0; i <= 100000; ++i){
		packets.push_back(packet());
		packets[i].acknowledged = false;
		packets[i].packet_data = (char*)"This is some data.";
	}
	
	CreateThread(NULL, 0, recieve_and_acknolwedge, (void*)NULL, 0, NULL);
	CreateThread(NULL, 0, send_packets, (void*) NULL, 0, NULL);

	Sleep(86400000); /* go to sleep for 86400 seconds (one day) */
	return 0;
}