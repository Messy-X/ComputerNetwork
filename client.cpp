#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include <pthread.h>

#include <queue>

#define MAXBUF 2048

using namespace std;
enum operation {CONNECT = 1, DISCONNECT, GETTIME, GETNAME, GETACTIVELIST, SEND, QUIT};
enum type {TIME = 1, NAME, LIST, MESG, INFO, INIT};

int Init();
void printMenu();
int mainFunc();
int mySelect();
void myConnect(string IP, int port);
void* receive_thread(void *);
void messageHandler();
void closeConnection();
int mySend(int socket, const char *buf, int len);

string IP;
int port;
int s_fd, op;
int mlen;
int i;
int isConnected = 0;
pthread_t thread;
queue <string> messageQueue;
pthread_mutex_t mutex_message;
pthread_mutex_t mutex_connect;

int main(int argc, char *argv[])
{
	pthread_mutex_init(&mutex_message, NULL);
	pthread_mutex_init(&mutex_connect, NULL);
    s_fd = Init();
    while(1)
    {
    	if(isConnected)
    	{
	    	if(mySelect())
	    	{
	    		pthread_mutex_lock(&mutex_message);
	    		messageHandler();
		    	pthread_mutex_unlock(&mutex_message);
	    	}
	    	else
	    		mainFunc();
	    }
	    else
	    	mainFunc();
	    cout << endl;
    }
	return 0;
}
int Init()
{
	int s_fd = socket(AF_INET,SOCK_STREAM,0);
	if(s_fd == -1)
	{
		cout << "Create socket error!" << endl;
		exit(-1);
	}
	return s_fd;
}
void printMenu()
{
	if(isConnected)
		cout << "Connecting to IP: " << IP << ", port: " << port << endl;
	cout << "======================Client's Memu======================" << endl;
	cout << "Connect to Server:          1" << endl;
	cout << "Disconnect to Server:       2 "<< endl;
	cout << "Get Server's Time:          3" << endl;
	cout << "Get Server's Name:          4" << endl;
	cout << "Get Active Connection list: 5" << endl;
	cout << "Send Message:               6" << endl;
	cout << "Quit:                       7" << endl;
	cout << "=========================================================" << endl;
	cout << "Your choice: ";
}
int mainFunc()
{
	printMenu();
	scanf("%d", &op);
	pthread_mutex_lock(&mutex_connect);
	if(!isConnected && op != 1)
	{
		pthread_mutex_unlock(&mutex_connect);
		cout << "Connection has not been built!" << endl;
		return 0;
	}
	pthread_mutex_unlock(&mutex_connect);
	switch(op)
	{
		case CONNECT:
		{
			if(isConnected)
			{
				cout << "Has building connection!" << endl;
				break;
			}
			cout << "input IP and port, a space between them" << endl;
			cin >> IP >> port;
			myConnect(IP, port);
			pthread_create(&thread, NULL, receive_thread, (void *)&s_fd);
			pthread_mutex_lock(&mutex_connect);
			isConnected = 1;
			pthread_mutex_unlock(&mutex_connect);
			break;
		}
		case DISCONNECT:
		{
			closeConnection();
			pthread_join(thread, NULL);
			break;
		}
		case GETTIME:
		{
			mySend(s_fd, "GETTIME", strlen("GETTIME"));
			break;
		}
		case GETNAME:
		{
			mySend(s_fd, "GETNAME", strlen("GETNAME"));
			break;
		}
		case GETACTIVELIST:
		{
			mySend(s_fd, "GETACTIVELIST", strlen("GETACTIVELIST"));
			break;
		}
		case SEND:
		{
			int id;
			char message[MAXBUF/2];
			char buf[MAXBUF] = "SEND";
			cout << "Input client ID and message:" << endl;
			cin >> id;
			cin.getline(message, MAXBUF/2 - 1);
			sprintf(buf + 4, "%d %s", id, message);
			mySend(s_fd, buf, strlen(buf));
			cout << "Press ENTER to continue";
			break;
		}
		case QUIT:
		{
			pthread_mutex_lock(&mutex_connect);
			if(isConnected)
			{
				pthread_mutex_unlock(&mutex_connect);
				closeConnection();
			}
			else
				pthread_mutex_unlock(&mutex_connect);
			exit(0);
			break;
		}
	}
	return 0;
}
int mySelect(int s_fd)
{
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	FD_SET(s_fd, &rfds);
    select(s_fd+1, &rfds, NULL, NULL, NULL);
    if(FD_ISSET(s_fd, &rfds))
    	return s_fd;
    else
    	return 0;
}
void myConnect(string IP, int port)
{
	struct sockaddr_in s_in;
	memset((void *)&s_in,0,sizeof(s_in));

    s_in.sin_family = AF_INET;
    s_in.sin_port = htons(port);
    inet_pton(AF_INET, IP.c_str(),(void *)&s_in.sin_addr);

    if(connect(s_fd,(struct sockaddr *)&s_in,sizeof(s_in)) == -1 )
	{
		cout << "Connect failed" << endl;
		exit(-1);
	}
	cout << "Connection built" << endl;
}
void* receive_thread(void *argv)
{
	char buf[MAXBUF];
	int mlen;
	s_fd = *(int *)argv;
	pthread_mutex_lock(&mutex_connect);
	while(isConnected)
	{
		pthread_mutex_unlock(&mutex_connect);
		mlen = recv(s_fd, buf, MAXBUF, MSG_DONTWAIT); //MSG_DONTWAIT
		if(mlen == 0)
			closeConnection();
		else if(mlen < 0)
			;
		else
		{
			buf[mlen] = 0;
			pthread_mutex_lock(&mutex_message);
			messageQueue.push(buf);
			pthread_mutex_unlock(&mutex_message);
		}
		pthread_mutex_lock(&mutex_connect);
	}
	pthread_mutex_unlock(&mutex_connect);
}
void messageHandler()
{
	string message;
	message = messageQueue.front();
	messageQueue.pop();
	string start = message.substr(0, 4);
	string end = message.substr(message.size()-3);
	message = message.substr(4, message.size()-7);
	if(end != "END")
	{
		cout << message << endl;
		cout << "Invalid message!" << endl << "Press ENTER to continue";
		return;
	}
	if(start == "TIME")
	{
		cout << "Receive response time: " << message << endl;
	}
	else if(start == "NAME")
	{
		cout << "Receive response: host name is " << message << endl;
	}
	else if(start == "LIST")
	{
		cout << "Receive response: active list is (with format: [ID] [IP] [port])" << endl << message << endl;
	}
	else if(start == "MESG")
	{
		cout << "Receive message from client with id: " << message << endl;
	}
	else if(start == "INFO")
	{
		cout << "Receive response from server: " << message << endl;
	}
	else if(start == "INIT")
	{
		cout << "Receive initialization message: " << message << endl;
	}
	else
	{
		cout << message << endl;
		cout << "Invalid message!" << endl;
	}
	cout << "Press ENTER to continue";
}
void closeConnection()
{
	pthread_mutex_lock(&mutex_connect);
	isConnected = 0;
	close(s_fd);
	pthread_mutex_unlock(&mutex_connect);
	cout << "Connection closed!" << endl;
	s_fd = Init();
}
int mySend(int socket, const char *buf, int len)
{
	char tmp[MAXBUF];
	int index = 5, i = 0;
	strcpy(tmp, "START");
	while(len--)
		tmp[index++] = buf[i++];
	strcpy(tmp + index, "END");
	index += 3;
	return send(socket, tmp, index, 0);
}
int mySelect() //0: keyboard, 1: message
{
	const int keyboard = 0;
	struct timeval time;
	fd_set rfds;
	while(1)
	{
		time = {0, 0};
		FD_ZERO(&rfds);
		FD_SET(keyboard, &rfds);
		if(select(keyboard+1, &rfds, NULL, NULL, &time) > 0)
			return 0;
		else
		{
    		pthread_mutex_lock(&mutex_message);
    		if(!messageQueue.empty())
    		{
    			pthread_mutex_unlock(&mutex_message);
    			return 1;
    		}
	    	pthread_mutex_unlock(&mutex_message);
		}
	}
}