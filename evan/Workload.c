#include <stdio.h>
#include <string.h>
#include<unistd.h>
#include<stdlib.h>
#include<netdb.h>
#include<sys/socket.h>
#include<arpa/inet.h>

//gcc -Wall Workload.c -o Workload -lpthread

int sendlog(char *log);

const char *loghost = "localhost", *logport = "44429";

int main(int argc, char ** argv){
	FILE *wl;
	char *op = malloc(100);
	char *commands = argv[1];
	puts(commands);

	const char *hostname = "localhost", *port = "44422";
	int sock;
	struct addrinfo serverSide,*serverInfo;
	char *server_reply = malloc(200);
	
	if ((wl = fopen(commands, "r")) == NULL)
	{
		perror ("Error opening file");
		return 1;
	}
	
	//struct timeval to;
	//to.tv_sec = 1;
	//to.tv_usec = 0;  // Not init'ing this can cause strange errors
	
	memset(&serverSide, 0, sizeof serverSide);
	serverSide.ai_family = AF_INET;
	serverSide.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(hostname,port,&serverSide,&serverInfo) != 0){
		printf("Get addr error\n");
	}

	sock = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	if (sock == -1)
	{
		printf("Could not create socket");
		return 1;
	}
	
	//setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&to,sizeof(struct timeval));

	if (connect(sock, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}
     
	puts("Connected\n");
/*
	while( recv(sock , server_reply , 100 , 0) > 0)
	{
		puts(server_reply);
	}
*/
	int read_size;
	//keep communicating with server
	while( fgets (op , 100 , wl) != NULL )
	{
		int len = (int)strlen(op);
		//if(len < 5) continue;
		op[len-1] = '\0';
		puts(op);
		//Send some data
		if( send(sock, op, len, 0) < 0)
		{
			puts("Send failed");
			break;
		}		
		
		//Receive a reply from the server
		if( (read_size = recv(sock, server_reply, 200 , 0)) < 0)
		{
			puts("recv failed");
			break;
		}
		server_reply[read_size] = '\0';
		puts(server_reply);

	}
     
    	close(sock);
	fclose (wl);
	
	return 0;
}

int sendlog(char *log)
{
	struct addrinfo serverSide,*serverInfo;

	memset(&serverSide, 0, sizeof(serverSide));
	serverSide.ai_family = AF_INET;
	serverSide.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(loghost,logport,&serverSide,&serverInfo) != 0){
		puts("Get addr error\n");
		return 1;
	}

	int sock = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	if (sock == -1)
	{
		puts("Could not create socket");
		return 1;
	}

	if (connect(sock, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}

	//Send some data
	if( send(sock, log, strlen(log), 0) < 0)
	{
		puts("Send failed");
		return 1;
	}

	free(serverInfo);
	close(sock);
	return 0;
}
