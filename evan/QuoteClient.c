#include<stdio.h> //printf
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include<unistd.h>
#include<stdlib.h>
#include<netdb.h>
#include<pthread.h>
#include<time.h>

//gcc -Wall QuoteClient.c -o QuoteClient -lpthread

#define CHAR_LIM 0x80

typedef struct quote_contents
{
	char *price;
	char *sym;
	char *user;
	//unsigned long long ts;
	char *key;
	//char *qs;
	long long ts;
} quote;

struct thread_args
{
	struct addrinfo *quoteServerInfo;
	int *clientSock;
	quote **cache;
	int *cacheLocked;
	long int *logSock;
};
void sendlog(int sock, char *log);

const char *loghost = "localhost", *logport = "44429";

//the thread function
void *connection_handler(void *);
int commaSep(char *in, char *out[5]);

int main(int argc , char *argv[])
{
	quote **cache = malloc(CHAR_LIM*CHAR_LIM*CHAR_LIM*sizeof(void *));
	int cache_locked = 0;


/*******************************************************************/
//Client setup
/*******************************************************************/
	const char *hostname = "quoteserve.seng.uvic.ca", *port = "4445";
	struct addrinfo serverSide,*serverInfo, *logInfo;
	long int *logSock = malloc(sizeof(long int));

	memset(&serverSide, 0, sizeof(serverSide));
	serverSide.ai_family = AF_INET;
	serverSide.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(hostname,port,&serverSide,&serverInfo) != 0){
		printf("Get addr error\n");
	}

	if(getaddrinfo(loghost,logport,&serverSide,&logInfo) != 0){
		puts("Get addr error\n");
		return 1;
	}

	*logSock = socket(logInfo->ai_family, logInfo->ai_socktype, logInfo->ai_protocol);
	if (*logSock == -1)
	{
		puts("Could not create socket");
		return 1;
	}

	if (connect(*logSock, logInfo->ai_addr, logInfo->ai_addrlen) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}

/*******************************************************************/
//Server setup
/*******************************************************************/

	int socket_desc , client_sock , c , *new_sock;
	struct sockaddr_in server , client;

	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
		return 1;
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 44420 );

	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");
	
	//Listen
	listen(socket_desc , 256);     

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);

	while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
	{
		puts("Connection accepted");
		
		pthread_t sniffer_thread;
		new_sock = malloc(sizeof(int));
		*new_sock = client_sock;

		struct thread_args cl = {serverInfo, new_sock, cache, &cache_locked, logSock};
		 
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) &cl) < 0)
		{
			perror("could not create thread");
			return 1;
		}
		 
		//Now join the thread , so that we dont terminate before the thread
		//pthread_join( sniffer_thread , NULL);
		puts("Handler assigned");
	}

	if (client_sock < 0)
	{
		perror("accept failed");
		return 1;
	}

	close(socket_desc);
	return 0;
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *args)
{
	struct addrinfo *serverInfo = ((struct thread_args *)args)->quoteServerInfo;
	quote **cache = ((struct thread_args *)args)->cache;
	int client_sock = *(((struct thread_args *)args)->clientSock);
	int *cache_locked = ((struct thread_args *)args)->cacheLocked;
	long int *logSock = ((struct thread_args *)args)->logSock;

	int quote_address;
	int read_size;
	//long long ta = 0, to = 55000; //310000 for check cache on first through
	//long t2, t1 = time(NULL);
	char *client_message = malloc(100);

	puts("Thread initialized");
	
	//Receive a message from client
	while( (read_size = recv(client_sock , client_message , 100 , 0)) > 0 )
	{
		client_message[read_size] = '\n';
		quote_address = (int)*(client_message);
		if(read_size > 1) quote_address = quote_address*CHAR_LIM + (int)*(client_message + 1);
		if(read_size > 2) quote_address = quote_address*CHAR_LIM + (int)*(client_message + 2);

//printf("%d\n", quote_address);
		if(cache[quote_address] != NULL && ( (long long)time(NULL) - cache[quote_address]->ts) < 60000)
		{

				puts("In Cache");
				quote q = *(*(cache + quote_address));
				char *reply = malloc(100);
				read_size = sprintf(reply, "%s,%s,%s,%llu,%s", q.price, q.sym, q.user, q.ts, q.key);
				send(client_sock, reply, read_size + 1, 0);
				free(reply);
		}
		else
		{
			int quote_sock = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
			if (quote_sock == -1)
			{
				printf("Could not create socket");
				break;
			}

			if (connect(quote_sock, serverInfo->ai_addr, serverInfo->ai_addrlen) < 0)
			{
				perror("connect failed. Error");
				break;
			}

			//Send some data
			if( send(quote_sock, client_message, read_size + 1, 0) < 0)
			{
				puts("Send failed");
				break;
			}

			char *server_reply = malloc(100);
			//Receive a reply from the server
			//t1 = time(NULL);

			if( (read_size = recv(quote_sock, server_reply, 100, 0) ) < 0)
			{

				puts("recv failed");
			}
			else
			{
				//t2 = time(NULL);
				//ta = 1000*((long long)(t1 + t2)>>1);

				if(client_message[read_size - 1] == '\0') read_size--;
				server_reply[read_size] = '\0';
				//puts("Hit Quote Server");
				//puts(server_reply);

				char * s = malloc(snprintf(NULL, 0, "quote,%lld,%s,%ld,%s", 1000*(long long)time(NULL), "LOAD1",(long int)1,server_reply) + 1);
				sprintf(s, "quote,%lld,%s,%ld,%s", 1000*(long long)time(NULL), "LOAD1",(long int)1,server_reply);
				sendlog(*logSock, s);
				free(s);

				char *newQuote[5];

				if(commaSep(server_reply, newQuote) < 0)
				{
					break;
				}

				quote *q = malloc(sizeof(quote));
				cache[quote_address] = q;
								

				while(*cache_locked == 1) usleep(1);
				*cache_locked = 1;

				q->price = newQuote[0];
				q->sym = newQuote[1];
				q->user = newQuote[2];
				//(*q).ts = strtoll(newQuote[3],NULL,10);
				q->ts = (long long)time(NULL);
				q->key = newQuote[4];

				free(newQuote[3]);
				*cache_locked = 0;

				if( send(client_sock, server_reply, read_size + 1, 0) < 0)
				{
					puts("Send failed");
					break;
				}

			}
			free(server_reply);
			close(quote_sock);
		}
	}

	if(read_size == 0)
	{
		puts("Client disconnected");
		fflush(stdout);
	}
	else if(read_size == -1)
	{
		perror("recv failed");
	}
	 
	//Free the socket pointer
	close(client_sock);
	free(((struct thread_args *)args)->clientSock);

	return 0;
}

int commaSep(char *in, char *out[5])
{
	if(in == NULL) return -1;
	int i,count = 0,start = 0, len = strlen(in);

	for(i=0;i<len;i++)
	{
		if(in[i] == ',')
		{
			out[count] = malloc(i - start + 1);
			strncpy(out[count], &in[start], i - start);
			out[count][i-start] = '\0';
			count++;
			start = i + 1;
		}
	}
	out[count] = malloc(i - start + 1);
	strncpy(out[count], &in[start], i - start);
	out[count][i-start] = '\0';
	return 0;
}

void sendlog(int sock, char *log)
{
	//Send some data
	if( send(sock, log, strlen(log) + 1, 0) < 0)
	{
		puts("Send failed");
		return;
	}

	char *str = malloc(20);
	if( recv(sock, str, 20, 0) < 0)
	{
		puts("recv failed");
		return;
	}
	free(str);
	return;
}
