#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>
#include "packet.h"
#include "serial.h"
#include "serialize.h"
#include "constants.h"
#include <stack>
#include <unistd.h>

using namespace std;

#define PORT_NAME			"/dev/ttyACM0"
#define BAUD_RATE			B9600
int is_backtracking = 0;
int exitFlag=0;
sem_t _xmitSema;
stack<char> back_command1;
stack<uint32_t> back_command2;
stack<uint32_t> back_command3;
void handleError(TResult error)
{
	switch(error)
	{
		case PACKET_BAD:
			printf("ERROR: Bad Magic Number\n");
			break;

		case PACKET_CHECKSUM_BAD:
			printf("ERROR: Bad checksum\n");
			break;

		default:
			printf("ERROR: UNKNOWN ERROR\n");
	}
}

void handleStatus(TPacket *packet)
{
	printf("\n ------- VINCENT STATUS REPORT ------- \n\n");
	printf("Left Forward Ticks:\t\t%d\n", packet->params[0]);
	printf("Right Forward Ticks:\t\t%d\n", packet->params[1]);
	printf("Left Reverse Ticks:\t\t%d\n", packet->params[2]);
	printf("Right Reverse Ticks:\t\t%d\n", packet->params[3]);
	printf("Left Forward Ticks Turns:\t%d\n", packet->params[4]);
	printf("Right Forward Ticks Turns:\t%d\n", packet->params[5]);
	printf("Left Reverse Ticks Turns:\t%d\n", packet->params[6]);
	printf("Right Reverse Ticks Turns:\t%d\n", packet->params[7]);
	printf("Forward Distance:\t\t%d\n", packet->params[8]);
	printf("Reverse Distance:\t\t%d\n", packet->params[9]);
	printf("Marked locations w/ and w/o cue:\t\t%d\n", packet->params[10]);
	printf("Marked locations w/ audio cue:\t\t%d\n", packet->params[11]);
	printf("\n---------------------------------------\n\n");
}

void handleResponse(TPacket *packet)
{
	// The response code is stored in command
	switch(packet->command)
	{
		case RESP_OK:
			printf("Command OK\n");
		break;

		case RESP_STATUS:
			handleStatus(packet);
		break;

		default:
			printf("Arduino is confused\n");
	}
}

void handleErrorResponse(TPacket *packet)
{
	// The error code is returned in command
	switch(packet->command)
	{
		case RESP_BAD_PACKET:
			printf("Arduino received bad magic number\n");
		break;

		case RESP_BAD_CHECKSUM:
			printf("Arduino received bad checksum\n");
		break;

		case RESP_BAD_COMMAND:
			printf("Arduino received bad command\n");
		break;

		case RESP_BAD_RESPONSE:
			printf("Arduino received unexpected response\n");
		break;

		default:
			printf("Arduino reports a weird error\n");
	}
}

void handleMessage(TPacket *packet)
{
	printf("Message from Vincent: %s\n", packet->data);
}

void handlePacket(TPacket *packet)
{
	switch(packet->packetType)
	{
		case PACKET_TYPE_COMMAND:
				// Only we send command packets, so ignore
			break;

		case PACKET_TYPE_RESPONSE:
				handleResponse(packet);
			break;

		case PACKET_TYPE_ERROR:
				handleErrorResponse(packet);
			break;

		case PACKET_TYPE_MESSAGE:
				handleMessage(packet);
			break;
	}
}

void sendPacket(TPacket *packet)
{
	char buffer[PACKET_SIZE];
	int len = serialize(buffer, packet, sizeof(TPacket));

	serialWrite(buffer, len);
cout<< "the packet has been sent" << "\n";
}

void *receiveThread(void *p)
{
	char buffer[PACKET_SIZE];
	int len;
	TPacket packet;
	TResult result;
	int counter=0;

	while(1)
	{
		len = serialRead(buffer);
		counter+=len;
		if(len > 0)
		{
			result = deserialize(buffer, len, &packet);

			if(result == PACKET_OK)
			{
				counter=0;
				handlePacket(&packet);
			}
			else 
				if(result != PACKET_INCOMPLETE)
				{
					printf("PACKET ERROR\n");
					handleError(result);
				}
		}
	}
}
/*void back_track()//not used any more.
{
	char current1;
	uint32_t current2,current3;
	TPacket back_Packet;
	back_Packet.packetType = PACKET_TYPE_COMMAND;
	while (!back_command1.empty())
	{
		current1=back_command1.top();
		current2=back_command2.top();
		current3=back_command3.top();
		back_command1.pop();
		back_command2.pop();
		back_command3.pop();
		
		back_Packet.command=current1;
		back_Packet.params[0]=current2;
		back_Packet.params[1]=current3;
	cout<<back_Packet.command<<" "<< back_Packet.params[0]<< " "<<back_Packet.params[1]<<"\n";
		sendPacket(&back_Packet);
		usleep(2000000);
	}
	
}*/
void flushInput()
{
	char c;
	
	while((c = getchar()) != '\n' && c != EOF);

}

void getParams(TPacket *commandPacket)
{
	if (is_backtracking){
		commandPacket->params[0]=back_command2.top();
		commandPacket->params[1]=back_command3.top();
		back_command2.pop();
		back_command3.pop();
    }
	else{
		printf("Enter distance/angle in cm/degrees (e.g. 50) and power in %% (e.g. 75) separated by space or mark/beep.\n");
		printf("E.g. 50 75 means go at 50 cm at 75%% power for forward/backward, or 50 degrees left or right turn at 75%%  power,1 1 means mark and beep vol 1, 1 0 mark no beep\n");
		scanf("%d %d", &commandPacket->params[0], &commandPacket->params[1]);
		back_command2.push(commandPacket->params[0]);
		if (commandPacket->params[1]==0) back_command3.push(1);
		else back_command3.push(commandPacket->params[1]);
		flushInput();
    }
}

void sendCommand(char command)
{
	TPacket commandPacket;

	commandPacket.packetType = PACKET_TYPE_COMMAND;

	switch(command)
	{
		/*case 'x':
		case 'X':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_BACK;
			//back_track();
			break;*/
		case 'm':
		case 'M':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_MARK;
			sendPacket(&commandPacket);
			break;
		case 'f':
		case 'F':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_FORWARD;
			sendPacket(&commandPacket);
			break;

		case 'b':
		case 'B':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_REVERSE;
			sendPacket(&commandPacket);
			break;

		case 'l':
		case 'L':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_TURN_LEFT;
			sendPacket(&commandPacket);
			break;

		case 'r':
		case 'R':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_TURN_RIGHT;
			sendPacket(&commandPacket);
			break;

		case 's':
		case 'S':
			commandPacket.command = COMMAND_STOP;
			sendPacket(&commandPacket);
			break;

		case 'c':
		case 'C':
			commandPacket.command = COMMAND_CLEAR_STATS;
			commandPacket.params[0] = 0;
			sendPacket(&commandPacket);
			break;

		case 'g':
		case 'G':
			commandPacket.command = COMMAND_GET_STATS;
			sendPacket(&commandPacket);
			break;

		case 'q':
		case 'Q':
			exitFlag=1;
			break;

		default:
			printf("Bad command\n");

	}
}

int main()
{
	// Connect to the Arduino
	startSerial(PORT_NAME, BAUD_RATE, 8, 'N', 1, 5);

	// Sleep for two seconds
	printf("WAITING TWO SECONDS FOR ARDUINO TO REBOOT\n");
	sleep(2);
	printf("DONE\n");

	// Spawn receiver thread
	pthread_t recv;

	pthread_create(&recv, NULL, receiveThread, NULL);

	// Send a hello packet
	TPacket helloPacket;

	helloPacket.packetType = PACKET_TYPE_HELLO;
	sendPacket(&helloPacket);	

	while(!exitFlag)
	{
		if (is_backtracking){
			sendCommand(back_command1.top());
			back_command1.pop();
			sleep(3);
			if (back_command1.empty()) break;
        }
		else{
	 		char ch;
			char conver_c = 'x';
			printf("Command (f=forward, b=reverse, l=turn left, r=turn right, s=stop, c=clear stats, g=get stats q=exit)\n");
			scanf("%c", &ch);
			
		 if (ch=='f'||ch=='F') conver_c='b';
		 else if (ch=='b'||ch=='B') conver_c='f';
		 else if (ch=='l'||ch=='L') conver_c='r';
		 else if (ch=='R'||ch=='r') conver_c='l';
		 else if (ch=='M'||ch=='m') conver_c='m';
		 else if (ch=='x'||ch=='X'){
			is_backtracking = 1;
			continue;
			}	
			// Purge extraneous characters from input stream
	  		back_command1.push(conver_c);
			flushInput();
			sendCommand(ch);
		}
	
	
	}

	printf("Closing connection to Arduino.\n");
	endSerial();
}
