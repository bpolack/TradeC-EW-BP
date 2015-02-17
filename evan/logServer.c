#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<netdb.h>
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
#include<time.h>
#include "mxml.h"

//gcc -Wall logServer.c -o logServer -I/seng/seng462/group2/mxml/include -L/seng/seng462/group2/mxml/lib -lmxml -lpthread
//gcc -Wall logServer.c -o logServer -I/home/evan/seng462/mxml/include -L/home/evan/seng462/mxml/lib -lmxml -lpthread

typedef struct queue_element
{
	char *clog;
	struct queue_element *next;
} queue_element;

typedef struct queue
{
	queue_element *start;
	queue_element *end;
	long int length;
	long int busy;
} queue;

struct conn_args
{
	queue *logs;
	long int *clientSock;
};

struct log_args
{
	queue *logs;
};

//the thread function
void *connection_handler(void *);
void *log_handler(void *);
//long int commaSep(char *in, char **out);
void cenq(queue *q, char *str, long int strsize);
char *cdeq(queue *q);

mxml_node_t *xml;  
mxml_node_t *logType;   

mxml_node_t *userCommand;
mxml_node_t *accountTransaction;
mxml_node_t *systemEvent;
mxml_node_t *quoteServer;
mxml_node_t *errorEvent;
mxml_node_t *debugEvent;

mxml_node_t *timestamp;
mxml_node_t *server;
mxml_node_t *transactionNum;
mxml_node_t *command;
mxml_node_t *username;
mxml_node_t *stockSymbol;
mxml_node_t *filename;
mxml_node_t *funds;
mxml_node_t *price;
mxml_node_t *quoteServerTime;
mxml_node_t *cryptokey;
mxml_node_t *action;
mxml_node_t *errorMessage;
mxml_node_t *debugMessage;
 
int main(int argc , char *argv[])
{	
	queue *logList = malloc(sizeof(queue));
	logList->start = malloc(sizeof(queue_element));
	logList->end = logList->start;
	logList->length = 0;
	logList->busy = 0;

	pthread_t buffer_thread;
	struct log_args logs = {logList};
		 
	if( pthread_create( &buffer_thread , NULL ,  log_handler , (void*) &logs) < 0)
	{
		perror("could not create thread");
		return 1;
	}

	xml = mxmlNewXML("1.0");
	logType = mxmlNewElement(xml, "log");

/*******************************************************************/
//Server setup
/*******************************************************************/
	long int socket_desc , client_sock , c , *new_sock;
	struct sockaddr_in server , client;

	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		puts("Could not create socket");
		return 1;
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 44429 );

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
		new_sock = malloc(sizeof(long int));
		*new_sock = client_sock;

		struct conn_args conns = {logList, new_sock};
		 
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) &conns) < 0)
		{
			perror("could not create thread");
			return 1;
		}
		puts("Handler assigned");
	}

	if (client_sock < 0)
	{
		perror("accept failed");
		return 1;
	}

	return 0;
}
 
/*
 * This will handle connection for each client
 * */
void *connection_handler(void *connArgs)
{
	//Get the socket descriptor
	long int *sock = ((struct conn_args *)connArgs)->clientSock;
	queue *q = ((struct conn_args *)connArgs)->logs;
	
	long int read_size;
	char *client_message = malloc(500);
	const char *received = "Received";

	puts("Connection Initialized");
	//Receive a message from client
	while( (read_size = recv(*sock, client_message, 500, 0)) > 0 )
	{
		if(read_size < 5) continue;
		if(client_message[read_size - 1] == '\0') read_size--;	
		client_message[read_size] = '\0';
		while(q->busy) usleep(1);
		q->busy = 1;
		cenq(q, client_message, read_size + 1);
		q->busy = 0;
		send(*sock, received, 9, 0);
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
	close(*sock);
	free(sock);
	free(client_message);

	return 0;
}

void *log_handler(void *logArgs)
{
	queue *q = ((struct log_args *)logArgs)->logs;
	//queue_element *log;
	//char **logSep = malloc(sizeof(void *));
	const char *s = ",";
	char *in = malloc(10);
	
	while(1){
		free(in);
		while(!q->length) usleep(1);
		while(q->busy) usleep(1);
		q->busy = 1;
		in = cdeq(q);
		q->busy = 0;

		//logSep = malloc(sizeof(void *)*10);
		//commaSep(log->clog, logSep);

		//in = malloc(strlen(log->clog) + 1);
		//strcpy(in, log->clog);
		//free(log);

		//Separate comma separated string into log components
		if(in == NULL)
		{
			puts("String parse Error, value is null...");
			continue;
		}
		else
		{
			//puts(in);
			//puts("Adding new log...");
			char *token;
	
			// get the first token 
			token = strtok(in, s);
			if(token == NULL) continue;
			//printf( "Log Type %s Found\n", token );

		/*******************************************************************/						
		//Possible LogTypes: usercmd acctrans system quote error debug dumplog
		//Add nodes depending on LogType
		/*******************************************************************/
	
			//LogType usrcmd structure: "usercmd,timestamp,server,transactionNum,
			//						command,username,stockSymbol,filename,funds"
			if( !strcmp("usercmd", token) )
			{
				//Create XML nodes for userCommand logType
				userCommand = mxmlNewElement(logType, "userCommand");
	
				token = strtok(NULL, s);
				timestamp = mxmlNewElement(userCommand, "timestamp");
				mxmlNewText(timestamp, 0, token);
				token = strtok(NULL, s);
				server = mxmlNewElement(userCommand, "server");
				mxmlNewText(server, 0, token);
				token = strtok(NULL, s);
				transactionNum = mxmlNewElement(userCommand, "transactionNum");
				mxmlNewText(transactionNum, 0, token);
				token = strtok(NULL, s);
				command = mxmlNewElement(userCommand, "command");
				mxmlNewText(command, 0, token);
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					username = mxmlNewElement(userCommand, "username");
					mxmlNewText(username, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					stockSymbol = mxmlNewElement(userCommand, "stockSymbol");
					mxmlNewText(stockSymbol, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					filename = mxmlNewElement(userCommand, "filename");
					mxmlNewText(filename, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					funds = mxmlNewElement(userCommand, "funds");
					mxmlNewText(funds, 0, token);
				}
				continue;
			}
			//LogType quote structure: "quote,timestamp,server,transactionNum,
			//						price,stockSymbol,username,quoteServerTime,cryptokey"
			else if( !strcmp("quote", token) )
			{
				//Create XML nodes for quoteServer logType
		  		quoteServer = mxmlNewElement(logType, "quoteServer");
		
				token = strtok(NULL, s);
				timestamp = mxmlNewElement(quoteServer, "timestamp");
				mxmlNewText(timestamp, 0, token);
				token = strtok(NULL, s);
				server = mxmlNewElement(quoteServer, "server");
				mxmlNewText(server, 0, token);
				token = strtok(NULL, s);
				transactionNum = mxmlNewElement(quoteServer, "transactionNum");
				mxmlNewText(transactionNum, 0, token);
				token = strtok(NULL, s);
				price = mxmlNewElement(quoteServer, "price");
				mxmlNewText(price, 0, token);
				token = strtok(NULL, s);
				stockSymbol = mxmlNewElement(quoteServer, "stockSymbol");
				mxmlNewText(stockSymbol, 0, token);
				token = strtok(NULL, s);
				username = mxmlNewElement(quoteServer, "username");
				mxmlNewText(username, 0, token);
				token = strtok(NULL, s);
				quoteServerTime = mxmlNewElement(quoteServer, "quoteServerTime");
				mxmlNewText(quoteServerTime, 0, token);
				token = strtok(NULL, s);
				cryptokey = mxmlNewElement(quoteServer, "cryptokey");
				mxmlNewText(cryptokey, 0, token); 
				continue;
			}
			//LogType acctrans structure: "acctrans,timestamp,server,transactionNum,
			//						action,username,funds"
			else if( !strcmp("acctrans", token) )
			{
				//Create XML nodes for accountTransaction logType
				accountTransaction = mxmlNewElement(logType, "accountTransaction");

				token = strtok(NULL, s);
				timestamp = mxmlNewElement(accountTransaction, "timestamp");
				mxmlNewText(timestamp, 0, token);
				token = strtok(NULL, s);
				server = mxmlNewElement(accountTransaction, "server");
				mxmlNewText(server, 0, token);
				token = strtok(NULL, s);
				transactionNum = mxmlNewElement(accountTransaction, "transactionNum");
				mxmlNewText(transactionNum, 0, token);
				token = strtok(NULL, s);
				action = mxmlNewElement(accountTransaction, "action");
				mxmlNewText(action, 0, token);
				token = strtok(NULL, s);
				username = mxmlNewElement(accountTransaction, "username");
				mxmlNewText(username, 0, token);
				token = strtok(NULL, s);
				funds = mxmlNewElement(accountTransaction, "funds");
				mxmlNewText(funds, 0, token);
				continue;
			}
			//LogType system structure: "system,timestamp,server,transactionNum,
			//						command,username,stockSymbol,filename,funds"
			else if( !strcmp("system", token) )
			{
				//Create XML nodes for systemEvent logType
				systemEvent = mxmlNewElement(logType, "systemEvent");

				token = strtok(NULL, s);
				timestamp = mxmlNewElement(systemEvent, "timestamp");
				mxmlNewText(timestamp, 0, token);
				token = strtok(NULL, s);
				server = mxmlNewElement(systemEvent, "server");
				mxmlNewText(server, 0, token);
				token = strtok(NULL, s);
				transactionNum = mxmlNewElement(systemEvent, "transactionNum");
				mxmlNewText(transactionNum, 0, token);
				token = strtok(NULL, s);
				command = mxmlNewElement(systemEvent, "command");
				mxmlNewText(command, 0, token);
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					username = mxmlNewElement(systemEvent, "username");
					mxmlNewText(username, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					stockSymbol = mxmlNewElement(systemEvent, "stockSymbol");
					mxmlNewText(stockSymbol, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					filename = mxmlNewElement(systemEvent, "filename");
					mxmlNewText(filename, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					funds = mxmlNewElement(systemEvent, "funds");
					mxmlNewText(funds, 0, token);
				}
				continue;
			}
			//LogType error structure: "error,timestamp,server,transactionNum,
			//						command,username,stockSymbol,filename,funds,errorMessage"
			else if( !strcmp("error", token) )
			{
				//Create XML nodes for errorEvent logType
		  		errorEvent = mxmlNewElement(logType, "errorEvent");

				token = strtok(NULL, s);
				timestamp = mxmlNewElement(errorEvent, "timestamp");
				mxmlNewText(timestamp, 0, token);
				token = strtok(NULL, s);
				server = mxmlNewElement(errorEvent, "server");
				mxmlNewText(server, 0, token);
				token = strtok(NULL, s);
				transactionNum = mxmlNewElement(errorEvent, "transactionNum");
				mxmlNewText(transactionNum, 0, token);
				token = strtok(NULL, s);
				command = mxmlNewElement(errorEvent, "command");
				mxmlNewText(command, 0, token);
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					username = mxmlNewElement(errorEvent, "username");
					mxmlNewText(username, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					stockSymbol = mxmlNewElement(errorEvent, "stockSymbol");
					mxmlNewText(stockSymbol, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					filename = mxmlNewElement(errorEvent, "filename");
					mxmlNewText(filename, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					funds = mxmlNewElement(errorEvent, "funds");
					mxmlNewText(funds, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					errorMessage = mxmlNewElement(errorEvent, "errorMessage");
					mxmlNewText(errorMessage, 0, token);
				}
				continue;
			}
			//LogType debug structure: "debug,timestamp,server,transactionNum,
			//						command,username,stockSymbol,filename,funds,debugMessage"
			else if( !strcmp("debug", token) )
			{
				//Create XML nodes for debugEvent logType
				debugEvent = mxmlNewElement(logType, "debugEvent");

				token = strtok(NULL, s);
				timestamp = mxmlNewElement(debugEvent, "timestamp");
				mxmlNewText(timestamp, 0, token);
				token = strtok(NULL, s);
				server = mxmlNewElement(debugEvent, "server");
				mxmlNewText(server, 0, token);
				token = strtok(NULL, s);
				transactionNum = mxmlNewElement(debugEvent, "transactionNum");
				mxmlNewText(transactionNum, 0, token);
				token = strtok(NULL, s);
				command = mxmlNewElement(debugEvent, "command");
				mxmlNewText(command, 0, token);
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					username = mxmlNewElement(debugEvent, "username");
					mxmlNewText(username, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					stockSymbol = mxmlNewElement(debugEvent, "stockSymbol");
					mxmlNewText(stockSymbol, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					filename = mxmlNewElement(debugEvent, "filename");
					mxmlNewText(filename, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					funds = mxmlNewElement(debugEvent, "funds");
					mxmlNewText(funds, 0, token);
				}
				token = strtok(NULL, s);
				if ( strcmp(" ", token) )
				{
					debugMessage = mxmlNewElement(debugEvent, "debugMessage");
					mxmlNewText(debugMessage, 0, token);
				}
				continue;
			}
			//LogType DUMPLOG structure: "DUMPLOG,filename"
			//Exports the current node structure and save as "filename"
			else if( !strcmp("DUMPLOG", token) )
			{
				//Save XML to logfile.xml file
				FILE *fp;
				
				token = strtok(NULL, s);
				fp = fopen(token, "w");
				mxmlSaveFile(xml, fp, MXML_NO_CALLBACK);
				puts("XML File Creation Successful...");
				fclose(fp);
				continue;
			}
			else
			{
				puts("Command received does not exist.");
				continue;
			}
		}
	}

	return 0;
}
/*
long int commaSep(char *in, char **out)
{
	if(in == NULL) return 0;
	long int i=0,count = 0,start = 0, len = (long int)strlen(in);

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
	return ++count;
}*/

void cenq(queue *q, char *str, long int strsize)
{
	q->end->clog = malloc(strsize);
	strcpy(q->end->clog, str);
	q->end->next = malloc(sizeof(queue_element));
	q->end = q->end->next;
	q->length = q->length + 1;
	return;
}

char *cdeq(queue *q)
{
	if(q->start->clog == NULL) return NULL;
	char *strlog = malloc(strlen(q->start->clog) + 1);
	strcpy(strlog, q->start->clog);
	queue_element *element = q->start;
	q->start = q->start->next;
	element->next = NULL;
	q->length = q->length - 1;
	free(element);
	return strlog;
}
