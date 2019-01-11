#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include <pthread.h>

#include <list>
#include <algorithm>

//#define MAX_TIME 10		//Max waiting time
#define MAXBUF 2048
#define PORT 5431
#define MAXCLIENTS 100
using namespace std;

enum operation {CONNECT = 1, DISCONNECT, GETTIME, GETNAME, ACTIVELIST, SEND, QUIT};
enum type {TIME = 1, NAME, LIST, MESG, INFO, INIT};
typedef struct clientinfo
{
	int c_fd;
	struct sockaddr_in c_in;
	bool operator==(clientinfo b) const  
   	{  
	   return this->c_fd == b.c_fd;  
   	}
}clientinfo;

int Init(int port);
clientinfo Acception(int l_fd);
void* communication_thread(void *);
int mySend(int socket, const char *buf, int len, type m_type);

list<clientinfo> c_list;

int main(int argc, char *argv[])
{
	int l_fd, c_fd;
	int count = 0;
	pthread_t * threads[MAXCLIENTS];
	clientinfo clients_info[MAXCLIENTS];
	char ipstr[128];

    l_fd = Init(PORT);

    while(1)
	{
		clients_info[count] = Acception(l_fd);
		c_list.push_back(clients_info[count]);
		cout << "Connection from IP: " << inet_ntop(AF_INET, &clients_info[count].c_in.sin_addr.s_addr, ipstr, sizeof(ipstr)) 
			 << ". Port: " << ntohs(clients_info[count].c_in.sin_port) << endl;
		threads[count] = (pthread_t *)malloc(sizeof(pthread_t));
		pthread_create(threads[count], NULL, communication_thread, (void *)(clients_info+count));
		count++;
	}
	close(l_fd);

    return 0;
}
 int Init(int port)
 {
 	struct sockaddr_in s_in;//server address structure
    int l_fd;
    memset((void *)&s_in,0,sizeof(s_in));    
    s_in.sin_family = AF_INET;//IPV4 communication domain
    s_in.sin_addr.s_addr = INADDR_ANY;//accept any address
    s_in.sin_port = htons(port);//change port to netchar

    if((l_fd = socket(AF_INET,SOCK_STREAM,0)) == -1)//socket(int domain, int type, int protocol)
    {
    	cout << "Create socket error!" << endl;
		exit(-1);
    }

    if((bind(l_fd,(struct sockaddr *)&s_in,sizeof(s_in))) == -1)
	{
		cout << "Bind error!" << endl;
		exit(-1);
	}
	
    if(!listen(l_fd, MAXCLIENTS))//lisiening start
    	cout << "Initiallization succeeded!"<<endl;
    else
    {
    	cout << "Listen error!" << endl;
    	exit(-1); 
    }
    return l_fd;
}

clientinfo Acception(int l_fd)
{
    int c_fd;
	struct sockaddr_in c_in;//client address structure
    socklen_t client_len = sizeof(c_in);
    clientinfo client_info;

	cout<<"Waiting for connection."<<endl;
    c_fd = accept(l_fd, (struct sockaddr *)&c_in, &client_len);

    client_info.c_fd = c_fd;
    client_info.c_in = c_in;
    if(c_fd < 0)
    {
    	cout << "Acception error" << endl;
    	exit(-1);
    }
    return client_info;
}


void* communication_thread(void *argv)
{
	char buf[MAXBUF];
	int mlen;
	time_t now;
	struct tm *tm_now;
	char ipstr[128];
	clientinfo *client_info = (clientinfo *)argv;
	int c_fd = client_info->c_fd;
	struct sockaddr_in c_in = client_info->c_in;
	mySend(c_fd, "Hello, Client", strlen("Hello, Client"), INIT);
	//send(c_fd, "STARTHello, ClientEND", strlen("STARTHello, ClientEND"), 0);
	while(1)
	{
		mlen = recv(c_fd, buf, MAXBUF, 0);
		if(mlen == 0)
		{
			cout << "Disonnection from IP: " << inet_ntop(AF_INET, &c_in.sin_addr.s_addr, ipstr, sizeof(ipstr)) 
				 << ". Port: " << ntohs(c_in.sin_port) << endl;
			c_list.remove(*client_info);
			break;
		}
		buf[mlen] = 0;
		string message(buf);
		string start = message.substr(0, 5);
		string end = message.substr(message.size()-3);
		if(start != "START" || end != "END")
		{
			cout <<start<<end<<endl;
			cout << "Invalid message!" << endl;
			continue;
		}
		message = message.substr(5, message.size()-8);
		//cout << message << endl;
		if(message == "GETTIME")
		{
			cout << "GETTIME REQUEST" << endl;
			time(&now);
			tm_now = localtime(&now);
			sprintf(buf, "%d-%d-%d %d:%d:%d",
					tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec) ;
			mySend(c_fd, buf, strlen(buf), TIME);
		}
		if(message == "GETNAME")
		{
			cout << "GETNAME REQUEST" << endl;
			gethostname(buf, MAXBUF);
			mySend(c_fd, buf, strlen(buf), NAME);
		}
		if(message == "GETACTIVELIST")
		{
			cout << "GETACTIVELIST REQUEST" << endl;
			if(c_list.size() < 2)
			{
				mySend(c_fd, "No other clients!", strlen("No other clients!"), LIST);
				continue;
			}
			int index = 0;
			list<clientinfo>::iterator iter;
			for(iter = c_list.begin(); iter != c_list.end(); iter++)
			{
				if(iter->c_fd == c_fd)
					continue;
				sprintf(buf + index, "%d %s %d\n", iter->c_fd, inet_ntop(AF_INET, &iter->c_in.sin_addr.s_addr, ipstr, sizeof(ipstr)), ntohs(iter->c_in.sin_port));
				index = strlen(buf);
			}
			buf[index-1] = 0;
			mySend(c_fd, buf, strlen(buf), LIST);
		}
		if(message.substr(0, 4) == "SEND")
		{
			int id;
			char tmp[MAXBUF/2];
			sscanf(message.c_str()+4, "%d %s", &id, tmp);
			cout << "SEND REQUEST to ID: " << id << " with message: " << tmp << endl;
			clientinfo t;
			t.c_fd = id;
			list<clientinfo>::iterator iter;
			iter = find(c_list.begin(), c_list.end(), t);
			if(iter == c_list.end())
			{
				mySend(c_fd, "Client does not exist!", strlen("Client does not exist!"), INFO);
				cout << "Invalid SEND REQUEST!" << endl;
			}
			else
			{
				sprintf(buf, "%d\n%s", c_fd, tmp);
				mySend(id, buf, strlen(buf), MESG);
			}
		}
	}
	close(c_fd);
}

int mySend(int socket, const char *buf, int len, type m_type)
{
	char tmp[MAXBUF];
	int index = 4, i = 0;
	switch(m_type)
	{
		case TIME:
		{
			strcpy(tmp, "TIME");
			break;
		}
		case NAME:
		{
			strcpy(tmp, "NAME");
			break;
		}
		case LIST:
		{
			strcpy(tmp, "LIST");
			break;
		}
		case MESG:
		{
			strcpy(tmp, "MESG");
			break;
		}
		case INIT:
		{
			strcpy(tmp, "INIT");
			break;
		}
	}
	while(len--)
		tmp[index++] = buf[i++];
	strcpy(tmp + index, "END");
	index += strlen("END");
	tmp[index] = 0;
	cout << "------send " << tmp <<  endl;
	return send(socket, tmp, index, 0);
}