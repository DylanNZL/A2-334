#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>

#define semaphore HANDLE

void wait(semaphore h) {
	WaitForSingleObject(h, MAXLONG);
}

void signal(semaphore h) {
	ReleaseSemaphore(n,1,NULL);
}

semaphore create(int val) {
	return CreateSemaphore(NULL,(long)val, MAXLONG, NULL);
}

#define frameSize 4 //size of frame for Selective Repeat
#define sleepTime 1000 //RTT
#define timeOut 1200 //Wait time before resending packet

struct packetNode {
	unsigned long position; //Sequence number
	bool acknowledged;
	packetNode* next;
}

packetNode* frameStart;
packetNode* frameEnd;

semaphore mutex_canSend;
semaphore mutex_canAck;

unsigned long CALLBACK confirm_acknowledgement(void *ackPos) {
	unsigned long position = (unsigned) ackPos; //Position passed to thread
	
	packetNode* temp = frameStart; 
	
	while(position != temp->position) { //Find the packetNode with the matching position number
		temp = temp->next;
	}
	int moveFrame = 1;
	if(temp == frameStart) { //If we are looking at the front of the frame we need to check the next frame to see if it has already been acknoledge
		temp = frameStart->next;
		while(temp->acknowledged == true) {
			moveFrame++; //increase the number of spaces the frame can move up
			packetNode* deleteME = temp; //clean up memory
			temp = temp->next;
			delete deleteME;
		}
		frameStart = temp; //Re-assign the front of the frame to the first unacknoledged packetNode
		for(int i = 0; i < moveFrame; i++) {
			signal(mutex_canSend); //Allow the send Thread to send more packets
		}
	} else { //Not the first packet, we cannot move the frame
		wait(mutex_canAck);
		temp->acknowledged = true;
		signal(mutex_canAck;
	}
	
	//Delete thread - I need to look up how to do this, we didnt learn it in the paper. Martin kind of said to just leave it but not sure how this
	//will work on a large scale
}

unsigned long CALLBACK acknowledge_thread(void *p) {
	//Start the thread but put it to sleep just long enough for the first 4 packets to be sent
	Sleep(sleepTime - 100);
	while(1){
		//gets acknowledgement from server
		unsigned long position = //acknowledged position
		if(//position >= frameStart->position && position <= position+3){ //eg the position of the acknowlegement is within the frame
			unsigned long id;
			CreateThread(NULL, 0, confirm_acknowledgement, (void)* position, 0, &id);
		}
	}
}

unsigned long CALLBACK send_packets_thread(void* p) {
	while(1) {
		wait(mutex_canSend);
		//sendPakcet
	}	
}


int main() {
	mutex_canSend = create(frameSize);
	mutex_canAck = create(1);
}
