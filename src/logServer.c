#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<netdb.h>
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
#include<semaphore.h>
#include<sys/time.h>
#include "mxml.h"

//gcc -Wall logServer.c -o logServer -I/seng/seng462/group2/mxml/include -L/seng/seng462/group2/mxml/lib -lmxml -lpthread -O2

typedef struct queue_element
{
	char *clog;
	struct queue_element *next;
} queue_element;

typedef struct queue
{
	queue_element *start;
	queue_element *end;
	sem_t *length;
	pthread_mutex_t *busy;
} queue;

struct conn_args
{
	queue *logs;
	long int *clientSock;
	long int *init;
};

struct log_args
{
	queue *logs;
};

//the thread function
void *connection_handler(void *);
void *log_handler(void *);
long int delimSepNoCpy(char *in, char **out, char delim);
void lenq(queue *q, char *str, long int strsize);
char *ldeq(queue *q); 
long long getmTS();

const char *logOut = "logfile6.xml";
 
int main(int argc , char *argv[])
{
	queue *logList = calloc(1, sizeof(queue));
	logList->start = calloc(1, sizeof(queue_element));
	logList->end = logList->start;
	logList->busy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(logList->busy, NULL))
	{
		perror("mutex init failed");
		return 1;
	}
	logList->length = malloc(sizeof(sem_t));
	if (sem_init(logList->length, 0, 0))
	{
		perror("semaphore init failed");
		return 1;
	}

	pthread_t buffer_thread;
	struct log_args logArgs = {logList};
		 
	if( pthread_create( &buffer_thread , NULL ,  log_handler , (void*) &logArgs) < 0)
	{
		perror("could not create thread");
		return 1;
	}

/*******************************************************************/
//Server setup
/*******************************************************************/
	long int socketDesc , clientSock , c , *newSock;
	struct sockaddr_in server , client;
/*
	struct timeval to;
	to.tv_sec = 30;
	to.tv_usec = 0;
*/
	//Create socket
	socketDesc = socket(AF_INET , SOCK_STREAM , 0);
	if (socketDesc == -1)
	{
		puts("Could not create socket");
		return 1;
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 44420 );

	//Bind
	if( bind(socketDesc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");
	
	//Listen
	listen(socketDesc , 256);     

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);
	long int init;

	while( (clientSock = accept(socketDesc, (struct sockaddr *)&client, (socklen_t*)&c)) )
	{
		init = 0;
		puts("Connection accepted");
		//setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&to, sizeof(struct timeval));
		
		pthread_t sniffer_thread;
		newSock = malloc(sizeof(long int));
		*newSock = clientSock;

		struct conn_args conns = {logList, newSock, &init};
		 
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) &conns) < 0)
		{
			perror("could not create thread");
			return 1;
		}
		while(!init) usleep(1);
		puts("Handler assigned");
	}

	if (clientSock < 0)
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
	long int *init = ((struct conn_args *)connArgs)->init;
	queue *q = ((struct conn_args *)connArgs)->logs;
	
	long int readSize;
	char *clientMessage = malloc(500);
	const char *received = "REC";	

	*init = 1;

	puts("Connection Initialized");
	//Receive a message from client
	while( (readSize = recv(*sock, clientMessage, 500, 0)) > 0 )
	{
		puts(clientMessage);
		lenq(q, clientMessage, readSize);
		send(*sock, received, 4, 0);
	}

	if(readSize == 0)
	{
		puts("Client disconnected");
		fflush(stdout);
	}
	else if(readSize == -1)
	{
		perror("recv failed");
	}
	 
	//Free the socket pointer
	close(*sock);
	free(clientMessage);

	return 0;
}

void *log_handler(void *logArgs)
{
	queue *q = ((struct log_args *)logArgs)->logs;

	const char *logStart = "<?xml version=\"1.0\" encoding=\"utf-8\"?><log>";
	char *logEnd = malloc(8);
	sprintf(logEnd, "</log>%c", 0x0a);

	char *in;
	char **logSep = calloc(10,sizeof(char *));
	long int count = 0;
	long long lastUserCmd = getmTS();
	
	char *tmpStr = malloc(500);
	long int tmpStrLen;

	char *outStr = malloc(0x1000001);

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

	mxml_node_t *searchResult;

	mxml_node_t *xml = mxmlNewXML("1.0");  
	mxml_node_t *logRoot = mxmlNewElement(xml, "log");

	FILE *fp = fopen(logOut, "w");
	fputs(logStart, fp);
	fputs(logEnd, fp);
	fclose(fp);
	
	while(1){
		in = ldeq(q);

		/*******************************************************************/						
		//Possible LogTypes: usercmd acctrans system quote error debug dumplog
		//Add nodes depending on LogType
		/*******************************************************************/

		if(!delimSepNoCpy(in, logSep, ','))
		{
			puts("String parse Error, empty string...");
			continue;
		}
		count++;
		//LogType usrcmd structure: "usercmd,timestamp,server,transactionNum,
		//						command,username,stockSymbol,filename,funds"
		if( !strcmp("userCmd", logSep[0]) )
		{
			//Create XML nodes for userCommand logType
			userCommand = mxmlNewElement(logRoot, "userCommand");

			timestamp = mxmlNewElement(userCommand, "timestamp");
			mxmlNewText(timestamp, 0, logSep[1]);

			server = mxmlNewElement(userCommand, "server");
			mxmlNewText(server, 0, logSep[2]);

			transactionNum = mxmlNewElement(userCommand, "transactionNum");
			mxmlNewText(transactionNum, 0, logSep[3]);

			command = mxmlNewElement(userCommand, "command");
			mxmlNewText(command, 0, logSep[4]);

			if (logSep[5][0] != ' ')
			{
				username = mxmlNewElement(userCommand, "username");
				mxmlNewText(username, 0, logSep[5]);
			}

			if (logSep[6][0] != ' ')
			{
				stockSymbol = mxmlNewElement(userCommand, "stockSymbol");
				mxmlNewText(stockSymbol, 0, logSep[6]);
			}

			if (logSep[7][0] != ' ')
			{
				filename = mxmlNewElement(userCommand, "filename");
				mxmlNewText(filename, 0, logSep[7]);
			}

			if (logSep[8][0] != ' ')
			{
				funds = mxmlNewElement(userCommand, "funds");
				mxmlNewText(funds, 0, logSep[8]);
			}
			lastUserCmd = getmTS();
		}
		//LogType quote structure: "quote,timestamp,server,transactionNum,
		//						price,stockSymbol,username,quoteServerTime,cryptokey"
		else if( !strcmp("quote", logSep[0]) )
		{
			//Create XML nodes for quoteServer logType
	  		quoteServer = mxmlNewElement(logRoot, "quoteServer");

			timestamp = mxmlNewElement(quoteServer, "timestamp");
			mxmlNewText(timestamp, 0, logSep[1]);

			server = mxmlNewElement(quoteServer, "server");
			mxmlNewText(server, 0, logSep[2]);

			transactionNum = mxmlNewElement(quoteServer, "transactionNum");
			mxmlNewText(transactionNum, 0, logSep[3]);

			price = mxmlNewElement(quoteServer, "price");
			mxmlNewText(price, 0, logSep[4]);

			stockSymbol = mxmlNewElement(quoteServer, "stockSymbol");
			mxmlNewText(stockSymbol, 0, logSep[5]);

			username = mxmlNewElement(quoteServer, "username");
			mxmlNewText(username, 0, logSep[6]);

			quoteServerTime = mxmlNewElement(quoteServer, "quoteServerTime");
			mxmlNewText(quoteServerTime, 0, logSep[7]);

			cryptokey = mxmlNewElement(quoteServer, "cryptokey");
			mxmlNewText(cryptokey, 0, logSep[8]);
		}
		//LogType acctrans structure: "acctrans,timestamp,server,transactionNum,
		//						action,username,funds"
		else if( !strcmp("accTrans", logSep[0]) )
		{
			//Create XML nodes for accountTransaction logType
			accountTransaction = mxmlNewElement(logRoot, "accountTransaction");

			timestamp = mxmlNewElement(accountTransaction, "timestamp");
			mxmlNewText(timestamp, 0, logSep[1]);

			server = mxmlNewElement(accountTransaction, "server");
			mxmlNewText(server, 0, logSep[2]);

			transactionNum = mxmlNewElement(accountTransaction, "transactionNum");
			mxmlNewText(transactionNum, 0, logSep[3]);

			action = mxmlNewElement(accountTransaction, "action");
			mxmlNewText(action, 0, logSep[4]);

			username = mxmlNewElement(accountTransaction, "username");
			mxmlNewText(username, 0, logSep[5]);

			funds = mxmlNewElement(accountTransaction, "funds");
			mxmlNewText(funds, 0, logSep[6]);
		}
		//LogType system structure: "system,timestamp,server,transactionNum,
		//						command,username,stockSymbol,filename,funds"
		else if( !strcmp("system", logSep[0]) )
		{
			//Create XML nodes for systemEvent logType
			systemEvent = mxmlNewElement(logRoot, "systemEvent");

			timestamp = mxmlNewElement(systemEvent, "timestamp");
			mxmlNewText(timestamp, 0, logSep[1]);

			server = mxmlNewElement(systemEvent, "server");
			mxmlNewText(server, 0, logSep[2]);

			transactionNum = mxmlNewElement(systemEvent, "transactionNum");
			mxmlNewText(transactionNum, 0, logSep[3]);

			command = mxmlNewElement(systemEvent, "command");
			mxmlNewText(command, 0, logSep[4]);

			if (logSep[5][0] != ' ')
			{
				username = mxmlNewElement(systemEvent, "username");
				mxmlNewText(username, 0, logSep[5]);
			}

			if (logSep[6][0] != ' ')
			{
				stockSymbol = mxmlNewElement(systemEvent, "stockSymbol");
				mxmlNewText(stockSymbol, 0, logSep[6]);
			}

			if (logSep[7][0] != ' ')
			{
				filename = mxmlNewElement(systemEvent, "filename");
				mxmlNewText(filename, 0, logSep[7]);
			}

			if (logSep[8][0] != ' ')
			{
				funds = mxmlNewElement(systemEvent, "funds");
				mxmlNewText(funds, 0, logSep[8]);
			}
		}
		//LogType error structure: "error,timestamp,server,transactionNum,
		//						command,username,stockSymbol,filename,funds,errorMessage"
		else if( !strcmp("error", logSep[0]) )
		{
			//Create XML nodes for errorEvent logType
	  		errorEvent = mxmlNewElement(logRoot, "errorEvent");

			timestamp = mxmlNewElement(errorEvent, "timestamp");
			mxmlNewText(timestamp, 0, logSep[1]);

			server = mxmlNewElement(errorEvent, "server");
			mxmlNewText(server, 0, logSep[2]);

			transactionNum = mxmlNewElement(errorEvent, "transactionNum");
			mxmlNewText(transactionNum, 0, logSep[3]);

			command = mxmlNewElement(errorEvent, "command");
			mxmlNewText(command, 0, logSep[4]);

			if (logSep[5][0] != ' ')
			{
				username = mxmlNewElement(errorEvent, "username");
				mxmlNewText(username, 0, logSep[5]);
			}

			if (logSep[6][0] != ' ')
			{
				stockSymbol = mxmlNewElement(errorEvent, "stockSymbol");
				mxmlNewText(stockSymbol, 0, logSep[6]);
			}

			if (logSep[7][0] != ' ')
			{
				filename = mxmlNewElement(errorEvent, "filename");
				mxmlNewText(filename, 0, logSep[7]);
			}

			if (logSep[8][0] != ' ')
			{
				funds = mxmlNewElement(errorEvent, "funds");
				mxmlNewText(funds, 0, logSep[8]);
			}

			if (logSep[9][0] != ' ')
			{
				errorMessage = mxmlNewElement(errorEvent, "errorMessage");
				mxmlNewText(errorMessage, 0, logSep[9]);
			}
		}
		//LogType debug structure: "debug,timestamp,server,transactionNum,
		//						command,username,stockSymbol,filename,funds,debugMessage"
		else if( !strcmp("debug", logSep[0]) )
		{
			//Create XML nodes for debugEvent logType
			debugEvent = mxmlNewElement(logRoot, "debugEvent");

			timestamp = mxmlNewElement(debugEvent, "timestamp");
			mxmlNewText(timestamp, 0, logSep[1]);

			server = mxmlNewElement(debugEvent, "server");
			mxmlNewText(server, 0, logSep[2]);

			transactionNum = mxmlNewElement(debugEvent, "transactionNum");
			mxmlNewText(transactionNum, 0, logSep[3]);

			command = mxmlNewElement(debugEvent, "command");
			mxmlNewText(command, 0, logSep[4]);

			if (logSep[5][0] != ' ')
			{
				username = mxmlNewElement(debugEvent, "username");
				mxmlNewText(username, 0, logSep[5]);
			}

			if (logSep[6][0] != ' ')
			{
				stockSymbol = mxmlNewElement(debugEvent, "stockSymbol");
				mxmlNewText(stockSymbol, 0, logSep[6]);
			}

			if (logSep[7][0] != ' ')
			{
				filename = mxmlNewElement(debugEvent, "filename");
				mxmlNewText(filename, 0, logSep[7]);
			}

			if (logSep[8][0] != ' ')
			{
				funds = mxmlNewElement(debugEvent, "funds");
				mxmlNewText(funds, 0, logSep[8]);
			}

			if (logSep[9][0] != ' ')
			{
				debugMessage = mxmlNewElement(debugEvent, "debugMessage");
				mxmlNewText(debugMessage, 0, logSep[9]);
			}
		}
		//LogType DUMPLOG structure: "DUMPLOG,filename"
		//Exports the current node structure and save as "filename"
		else if( !strcmp("DUMPLOG", logSep[0]) )
		{
			if(logSep[1][0] == '.' || logSep[1][0] == '/')
			{
				if(getmTS() - lastUserCmd < 3000)
				{
					tmpStrLen = sprintf(tmpStr, "DUMPLOG,%s", logSep[1]);
					lenq(q, tmpStr, tmpStrLen + 1);
					free(in);
					count--;
					usleep(1);
					continue;
				}

				fp = fopen(logOut, "r+");
				if(fp)
				{
					fseek(fp, -7, SEEK_END);
					mxmlSaveString(logRoot, outStr, 0x1000000, MXML_NO_CALLBACK);
					fputs(outStr + 5, fp);
				
					puts("XML File Creation Successful...");
					puts(logOut);

					mxmlDelete(logRoot);
					logRoot = mxmlNewElement(xml, "log");
					count = 0;
					fclose(fp);
				}
			}
			else
			{
				fp = fopen(logSep[2], "w");
				if(fp)
				{
					char *tStr = outStr + 43; //43 = strlen(logStart)
					strcpy(outStr, logStart);
					searchResult = logRoot;

					for(	searchResult = mxmlFindElement(searchResult, logRoot, "username", NULL, NULL, MXML_DESCEND);
						searchResult != NULL;
						searchResult = mxmlFindElement(searchResult, logRoot, "username", NULL, NULL, MXML_DESCEND) 						){
						if(!strcmp(logSep[1], searchResult->child->value.text.string))
						{
							searchResult = searchResult->parent;
							tStr = tStr + mxmlSaveString(searchResult, tStr, 5000, MXML_NO_CALLBACK) - 1;
							searchResult = searchResult->next;
						}else searchResult = searchResult->parent->next;
					}
					
					strcpy(tStr, logEnd);
					fputs(outStr, fp);

					puts("userXML File Creation Successful...");
					puts(logSep[2]);
					fclose(fp);
				}
			}
		}
		else
		{
			puts("Command received does not exist.");
			puts(in);
		}
		free(in);
		
		if(count > 0x10000)
		{
			fp = fopen(logOut, "r+");
			if(fp)
			{
				fseek(fp, -7, SEEK_END);
				mxmlSaveString(logRoot, outStr, 0x1000000, MXML_NO_CALLBACK);
				fputs(outStr + 5, fp);
				
				puts("XML File Creation Successful...");
				puts(logOut);

				mxmlDelete(logRoot);
				logRoot = mxmlNewElement(xml, "log");
				count = 0;
				fclose(fp);
			}
		}
	}
	free(logEnd);
	free(logSep);
	free(tmpStr);

	return 0;
}

long int delimSepNoCpy(char *in, char **out, char delim)
{
	if(!in) return 0;
	long int i,count = 0,start = 0, len = strlen(in);
	if(!len) return 0;

	for(i=0;i<len;i++)
	{
		if(in[i] == delim)
		{
			in[i] = '\0';
			out[count++] = in + start;
			start = i + 1;
		}
	}
	out[count++] = in + start;
	return count;
}

void lenq(queue *q, char *str, long int strsize)
{
	pthread_mutex_lock(q->busy);
	q->end->clog = malloc(strsize);
	strcpy(q->end->clog, str);
	q->end->next = calloc(1, sizeof(queue_element));
	q->end = q->end->next;
	sem_post(q->length);
	pthread_mutex_unlock(q->busy);
	return;
}

char *ldeq(queue *q)
{
	sem_wait(q->length);
	pthread_mutex_lock(q->busy);
	if(!q->start->clog)
	{	
		pthread_mutex_unlock(q->busy);
		return NULL;
	}
	char *strlog = q->start->clog;
	queue_element *element = q->start;
	q->start = q->start->next;
	free(element);
	pthread_mutex_unlock(q->busy);
	return strlog;
}

long long getmTS()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	long long ts = ( ( (long long)(tv.tv_sec) ) * 1000) + ( ( (long long)(tv.tv_usec) )/1000);
	return ts;
//can also multiply by 1049 then shift right by 20 instead of dividing by 1000
}
