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

struct packet {
	bool acknowledged;
	char* packet_data;
	clock_t end_time;
};

vector<packet> packets;
unsigned long frame_start;
semaphore can_send;



unsigned long CALLBACK send_packets(void* n) {
	while(1) {
		wait(can_send);
		cout << "sending packet" << endl;
	}
	return 0;
}

unsigned long CALLBACK recieve_and_acknolwedge(void* n) {
	Sleep(3000);
	while(1) {
		cout << "Recvied acknowledgement" << endl;
		signal(can_send);
		Sleep(1000);
	}
	return 0;
}



int main() {
	cout << "stop" << endl;
	
	can_send = create(frame_size);
		
	CreateThread(NULL, 0, recieve_and_acknolwedge, (void*)NULL, 0, NULL);
	CreateThread(NULL, 0, send_packets, (void*) NULL, 0, NULL);
	
	
	Sleep(86400000); /* go to sleep for 86400 seconds (one day) */
	return 0;
}