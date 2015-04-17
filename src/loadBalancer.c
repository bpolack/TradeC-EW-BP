#include<stdio.h> //printf
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include<unistd.h>
#include<fcntl.h>
#include<sys/select.h>
#include<errno.h>
#include<stdlib.h>
#include<netdb.h>
#include<pthread.h>
#include<sys/time.h>
#include<semaphore.h>

//gcc -Wall loadBalancer.c -o loadBalancer -lpthread -O2

#define CHAR_LIM 26
#define NUM_LOG 1
#define quoteTo 28000

typedef struct quote_contents
{
	char *price;
	char *sym;
	char *user;
	char *key;
	long long ts;
	pthread_mutex_t busy;
} quote;

typedef struct quote_reply
{
	char *reply;
	pthread_mutex_t busy;
} quote_reply;

typedef struct log_queue_element
{
	char *log;
	struct log_queue_element *next;
} log_queue_element;

typedef struct log_queue
{
	struct log_queue *logs;
	struct log_queue_element *start;
	struct log_queue_element *end;
	sem_t *length;
	pthread_mutex_t *busy;
} log_queue;

typedef struct quote_queue_element
{
	char *request;
	long int requestLen;
	char *transID;
	long int quoteAddress;
	struct quote_contents *response;
	sem_t returned;
	struct quote_queue_element *next;
} quote_queue_element;

typedef struct quote_queue
{
	struct quote_queue_element *start;
	struct quote_queue_element *end;
	sem_t *length;
	pthread_mutex_t *busy;
} quote_queue;

struct get_quote_args
{
	struct log_queue *logs;	
	char **request;
	long int *requestSize;
	quote_reply *reply;
	sem_t *quoteStart;
	sem_t *quoteFin;
	pthread_mutex_t *threadReturned;
	long int *threadReturnedID;
	long int *go;
	char **transID;
	long int threadID;
	long int *init;
};

struct quote_args
{
	struct quote_queue *quotes;
	struct log_queue *logs;
	quote **cache;
};

struct conn_args
{
	struct quote_queue *quotes;
	struct log_queue *logs;
	quote **cache;
	long int *clientSock;
	long int *init;
};

struct log_args
{
	struct log_queue *logs;
};

const char *logServerPort = "44420";
const char *logServerHost[] = 
{
	"b145.seng.uvic.ca",
	"b146.seng.uvic.ca"
};
const char *quoteHost = "quoteserve.seng.uvic.ca";
char *quotePort;// = "4445";

void *connection_handler(void *connArgs);
void *quote_handler(void *quoteArgs);
void *log_handler(void *logArgs);
void lenq(log_queue *q, char *str, long int strsize);
char *ldeq(log_queue *q);
quote_queue_element *qenq(quote_queue *q, char *request, long int requestLen, char *transID, long int quoteAddress);
quote_queue_element *qdeq(quote_queue *q);
long int delimSep(char *in, char **out, char delim);
void *get_quote_thread(void *getQuoteArgs);
long long getmTS();
long int str2price(char *str);

long int maxRequests = 10;
long int timeOut = 30000;

int main(int argc , char *argv[])
{
	quotePort = malloc(5);
	strcpy(quotePort,argv[1]);
	puts(quotePort);

	long int i;
	quote *q;
	quote **cache = calloc(CHAR_LIM*CHAR_LIM*CHAR_LIM, sizeof(quote *));
	for(i=0;i<CHAR_LIM*CHAR_LIM*CHAR_LIM;i++)
	{
		q = calloc(1, sizeof(quote));
		if (pthread_mutex_init(&q->busy, NULL))
		{
			perror("mutex init failed");
			return 1;
		}
		cache[i] = q;
	}

	log_queue *logList = calloc(1, sizeof(log_queue));
	logList->start = calloc(1, sizeof(log_queue_element));
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

	quote_queue *quoteList = calloc(1, sizeof(quote_queue));
	quoteList->start = calloc(1, sizeof(quote_queue_element));
	quoteList->end = quoteList->start;
	quoteList->busy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(quoteList->busy, NULL))
	{
		perror("mutex init failed");
		return 1;
	}
	quoteList->length = malloc(sizeof(sem_t));
	if (sem_init(quoteList->length, 0, 0))
	{
		perror("semaphore init failed");
		return 1;
	}

	pthread_t log_thread;
	struct log_args logArgs = {logList};
		 
	if( pthread_create( &log_thread , NULL ,  log_handler , (void*) &logArgs) < 0)
	{
		perror("could not create thread");
		return 1;
	}

	struct quote_args quoteArgs = {quoteList, logList, cache};
	for(i=0;i<3;i++)
	{
		pthread_t quote_thread;	 
		if( pthread_create( &quote_thread , NULL ,  quote_handler , (void*) &quoteArgs) < 0)
		{
			perror("could not create thread");
			return 1;
		}
	}

/*******************************************************************/
//Server setup
/*******************************************************************/

	long int socketDesc , clientSock , c , *newSock;
	struct sockaddr_in loadBalServer;
/*
	struct timeval to;
	to.tv_sec = 30;
	to.tv_usec = 0;
*/
	//Create socket
	socketDesc = socket(AF_INET , SOCK_STREAM , 0);
	if (socketDesc == -1)
	{
		perror("Could not create socket");
		return 1;
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	loadBalServer.sin_family = AF_INET;
	loadBalServer.sin_addr.s_addr = INADDR_ANY;
	loadBalServer.sin_port = htons( 44421 );

	//Bind
	if( bind(socketDesc,(struct sockaddr *)&loadBalServer , sizeof(struct sockaddr)) < 0)
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

	while( (clientSock = accept(socketDesc, NULL, (socklen_t*)&c)) )
	{
		init = 0;
		puts("Connection accepted");
		//setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&to, sizeof(struct timeval));
		
		pthread_t sniffer_thread;
		newSock = malloc(sizeof(int));
		*newSock = clientSock;

		struct conn_args cl = {quoteList, logList, cache, newSock, &init};
		 
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) &cl) < 0)
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

	close(socketDesc);
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
	quote_queue *qq = ((struct conn_args *)connArgs)->quotes;
	quote **cache = ((struct conn_args *)connArgs)->cache;
	//log_queue *lq = ((struct conn_args *)connArgs)->logs;
	
	long int readSize;
	char *clientMessage = malloc(200);
	char *messageSep[4];
	char *reply = malloc(200);
	long int len;
	quote_queue_element *quoteResp;

	//char *userCmd;
	char *transID;
	long int quoteAddress;
	char *request;
	long int requestLen;
	quote *q;

	//char *logStr = malloc(500);
	//long int logStrLen;

	*init = 1;

	puts("Connection Initialized");
	//Receive a message from client
	while( (readSize = recv(*sock, clientMessage, 200, 0)) > 0 )
	{
		//Separate message into array messageSep[].
		delimSep(clientMessage, messageSep, '|');
		transID = messageSep[1];
		//userCmd = messageSep[2];
		request = messageSep[3];
		requestLen = strlen(request) + 1;

		//logStrLen = sprintf(logStr, "system,%lld,%s,%s,%s, , , , ", getmTS(), "LOAD1", transID, userCmd);
		//lenq(lq, logStr, logStrLen + 1);

		quoteAddress = ( ( (long int)(*request) ) - 65) % CHAR_LIM;
		quoteAddress = quoteAddress*CHAR_LIM + ( ( ( (long int)( *(request + 1) ) ) - 65) % CHAR_LIM);
		quoteAddress = quoteAddress*CHAR_LIM + ( ( ( (long int)( *(request + 2) ) ) - 65) % CHAR_LIM);

		q = cache[quoteAddress];
		
		if(!strncmp("QCR", messageSep[0], 3))
		{
			pthread_mutex_lock(&q->busy);
			if( getmTS() - q->ts < quoteTo) len = sprintf(reply, "%s,%s,%s,%llu,%s", q->price, q->sym, q->user, q->ts, q->key);
			else len = sprintf(reply, "NOT");
			pthread_mutex_unlock(&q->busy);
		}
		else if(!strncmp("QNR", messageSep[0], 3))
		{
			pthread_mutex_lock(&q->busy);
			
			if( getmTS() - q->ts < quoteTo)
			{
				len = sprintf(reply, "%s,%s,%s,%llu,%s", q->price, q->sym, q->user, q->ts, q->key);
				pthread_mutex_unlock(&q->busy);
			}
			else
			{
				pthread_mutex_unlock(&q->busy);
				quoteResp = qenq(qq, request, requestLen, transID, quoteAddress);		
				sem_wait(&quoteResp->returned);
				q = quoteResp->response;
				//sem_destroy(&quoteResp->returned);
				free(quoteResp);

				pthread_mutex_lock(&q->busy);
				len = sprintf(reply, "%s,%s,%s,%llu,%s", q->price, q->sym, q->user, q->ts, q->key);
				pthread_mutex_unlock(&q->busy);
			}
		}
		else len = sprintf(reply, "ERR");

		if( send(*sock, reply, len + 1, 0) < 0)
		{
			perror("Send failed");
			break;
		}
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
	free(reply);

	return 0;
}

void *quote_handler(void *quoteArgs)
{
	quote **cache = ((struct quote_args *)quoteArgs)->cache;
	quote_queue *qq = ((struct quote_args *)quoteArgs)->quotes;
	log_queue *lq = ((struct quote_args *)quoteArgs)->logs;

	long int quoteAddress;
	char *requestPointer;
	long int requestLen;
	quote_reply *serverReply;
	char *transID;

	quote_queue_element *quoteResp;

	quote *q;
	quote_reply **replies = malloc(sizeof(void *)*maxRequests);
	long int i;
	long int go = 0;
	int semVal;
	pthread_mutex_t threadReturned;
	long int threadReturnedID;

	sem_t quoteStart;
	if (sem_init(&quoteStart, 0, 0))
	{
		perror("semaphore init failed");
		pthread_exit(NULL);
	}
	sem_t quoteFin;
	if (sem_init(&quoteFin, 0, 1))
	{
		perror("semaphore init failed");
		pthread_exit(NULL);
	}
	
	pthread_t getQuoteThread[maxRequests];
	
	for(i=0;i<maxRequests;i++)
	{
		replies[i] = malloc(sizeof(quote_reply));
		quote_reply *qrTemp = replies[i];
		if (pthread_mutex_init(&qrTemp->busy, NULL))
		{
			perror("mutex init failed");
			pthread_exit(NULL);
		}
		qrTemp->reply = malloc(200);
		long int init = 0;

		struct get_quote_args cl = {lq, &requestPointer, &requestLen, qrTemp, &quoteStart, &quoteFin, &threadReturned, &threadReturnedID, &go, &transID, i, &init};

		if( pthread_create(&getQuoteThread[i], NULL, get_quote_thread, (void*) &cl) < 0)
		{
			perror("could not create thread");
			pthread_exit(NULL);
		}
		while(!init) usleep(1); //wait until initialized
		
		pthread_mutex_lock(&replies[i]->busy);
	}
	sem_wait(&quoteFin);
	puts("Thread initialized");
	
	//Receive a message from client
	while(1)
	{
		quoteResp = qdeq(qq);
		requestPointer = quoteResp->request;
		requestLen = quoteResp->requestLen;
		quoteAddress = quoteResp->quoteAddress;
		transID = quoteResp->transID;

		q = cache[quoteAddress];
		pthread_mutex_lock(&q->busy);

		//Allow quote threads to start requesting
		for(i=0;i<maxRequests;i++) pthread_mutex_unlock(&replies[i]->busy);
		for(i=0;i<maxRequests;i++) sem_post(&quoteStart);
		do{
			if(sem_getvalue(&quoteFin, &semVal)) pthread_exit(NULL);
			usleep(1);
		} while(semVal);

		for(i=0;i<maxRequests;i++)
		{
			while(!pthread_mutex_trylock(&replies[i]->busy))
			{
				pthread_mutex_unlock(&replies[i]->busy);
				usleep(1);
			}
		}
		go = 1;

		//Wait for at least one quote thread to return with quote
		sem_wait(&quoteFin);
		serverReply = replies[threadReturnedID];

		char *rep = serverReply->reply;

		//puts("Hit Quote Server");
		puts(serverReply->reply);

		char *newQuote[5];

		if(!delimSep(rep, newQuote, ','))
		{
			pthread_mutex_unlock(&q->busy);
			break;
		}

		free(q->price);
		free(q->sym);
		free(q->user);
		free(q->key);

		q->price = newQuote[0];
		q->sym = newQuote[1];
		q->user = newQuote[2];
		q->ts = getmTS();
		q->key = newQuote[4];

		free(newQuote[3]);

		quoteResp->response = q;
		sem_post(&quoteResp->returned);

		//Wait until all quote threads have returned
		for(i=0;i<maxRequests;i++) pthread_mutex_lock(&replies[i]->busy);
		for(i=0;i<maxRequests;i++) sem_trywait(&quoteFin);
		go = 0;

		pthread_mutex_unlock(&q->busy);
	}

	for(i=0;i<maxRequests;i++)
	{
		pthread_join(getQuoteThread[i], NULL);
		free(replies[i]->reply);
	}
	free(replies);

	return 0;
}

void *get_quote_thread(void *getQuoteArgs)
{
	log_queue *lq = ((struct get_quote_args *)getQuoteArgs)->logs;
	char **request = ((struct get_quote_args *)getQuoteArgs)->request;
	long int *requestSize = ((struct get_quote_args *)getQuoteArgs)->requestSize;
	quote_reply *reply = ((struct get_quote_args *)getQuoteArgs)->reply;
	sem_t *quoteStart = ((struct get_quote_args *)getQuoteArgs)->quoteStart;
	sem_t *quoteFin = ((struct get_quote_args *)getQuoteArgs)->quoteFin;
	pthread_mutex_t *threadReturned = ((struct get_quote_args *)getQuoteArgs)->threadReturned;
	long int *threadReturnedID = ((struct get_quote_args *)getQuoteArgs)->threadReturnedID;
	long int *go = ((struct get_quote_args *)getQuoteArgs)->go;
	char **transID = ((struct get_quote_args *)getQuoteArgs)->transID;
	long int threadID = ((struct get_quote_args *)getQuoteArgs)->threadID;
	long int *init = ((struct get_quote_args *)getQuoteArgs)->init;

	long int quoteSock;
	long int readSize;
	char *rep = reply->reply;

	char *logStr = malloc(500);
	long int logStrLen;
	long int connected = 0;

	struct addrinfo serverSide, *quoteHostInfo;

	memset(&serverSide, 0, sizeof(serverSide));
	serverSide.ai_family = AF_INET;
	serverSide.ai_socktype = SOCK_STREAM;

	struct timeval to;
	to.tv_sec = 0;
	to.tv_usec = 20000;

	fd_set set;
	FD_ZERO(&set);
/*
	struct timespec to;
	to.tv_sec = 0;
	to.tv_nsec = 100;
*/
	long int count;
	int semVal;
	long int sockStat;
	pthread_mutex_lock(&reply->busy);

	*init = 1;

	while(1)
	{
		count = 0;
		if(sem_getvalue(quoteFin, &semVal)) pthread_exit(NULL);
		if(semVal)
		{
			pthread_mutex_unlock(&reply->busy);
			sem_wait(quoteStart);
			pthread_mutex_lock(&reply->busy);
			while(!*go) usleep(1);
		}

		if(!connected)
		{
			if(getaddrinfo(quoteHost,quotePort,&serverSide,&quoteHostInfo))
			{
				perror("Get Quote Server addr error");
				connected = 0;
				usleep(1);
				continue;
			}
			connected = 1;
		}

		quoteSock = socket(quoteHostInfo->ai_family, quoteHostInfo->ai_socktype, quoteHostInfo->ai_protocol);
		if (quoteSock == -1)
		{
			perror("Could not create socket");
			connected = 0;
			continue;
		}
		//setsockopt(quoteSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&to, sizeof(struct timeval));

		fcntl(quoteSock, F_SETFL, O_NONBLOCK);
		FD_SET(quoteSock, &set);
    
		if (connect(quoteSock, quoteHostInfo->ai_addr, quoteHostInfo->ai_addrlen) < 0)
		{
			if (errno != EINPROGRESS)
			{
				perror("connect failed. Error");
				close(quoteSock);
				connected = 0;
				continue;
			}
		}
		sockStat = select(quoteSock+1, NULL, &set, NULL, &to);

		if(sockStat < 0)
		{
			perror("connect failed. Error");
			close(quoteSock);
			connected = 0;
			continue;
		}
		else if(!sockStat)
		{
			close(quoteSock);
			//do{
			//	if(sem_getvalue(quoteFin, &semVal)) pthread_exit(NULL);
			//	usleep(1);
			//}while(!semVal);
			continue;
		}
		FD_CLR(quoteSock, &set);
		fcntl(quoteSock, F_SETFL, 0);

		if( send(quoteSock, *request, *requestSize, 0) < 0)
		{
			perror("Send failed");
			close(quoteSock);
			connected = 0;
			continue;
		}

		while( (readSize = recv(quoteSock, rep, 200, MSG_DONTWAIT) ) < 0)
		{			
			if(sem_getvalue(quoteFin, &semVal)) pthread_exit(NULL);
			if(!semVal && count<timeOut)
			{
				count++;
				usleep(1);
				continue;
			}/*
			if(sem_timedwait(quoteFin, &to) && count<10000)
			{
				count++;
				continue;
			}*/
			else
			{
				count = -1;
				//puts("Timed out");
				break;
			}
		}

		close(quoteSock);
		if(count == -1) continue;

		if(rep[readSize-1] == '\0') readSize--;
		if(rep[readSize-1] == '\n') readSize--;
		rep[readSize] = '\0';

		if(str2price(rep) == -1)
		{
			//logStrLen = sprintf(logStr, "quote,%lld,%s,%s,%s", getmTS(), "LOAD1",*transID,rep);
			//lenq(lq, logStr, logStrLen + 1);
		}
		else
		{
			logStrLen = sprintf(logStr, "quote,%lld,%s,%s,%s", getmTS(), "LOAD1",*transID,rep);
			lenq(lq, logStr, logStrLen + 1);
		}

		if(!pthread_mutex_trylock(threadReturned))
		{
			*threadReturnedID = threadID;
			sem_post(quoteFin);
			pthread_mutex_unlock(threadReturned);
		}
	}
	free(logStr);

	return 0;
}

long long getmTS()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	long long ts = ( ( (long long)(tv.tv_sec) ) * 1000) + ( ( (long long)(tv.tv_usec) )/1000);
	return ts;
//can also multiply by 1049 then shift right by 20 instead of dividing by 1000
}

long int delimSep(char *in, char **out, char delim)
{
	if(!in) return 0;
	long int i,count = 0,start = 0, len = strlen(in);
	if(!len) return 0;

	for(i=0;i<len;i++)
	{
		if(in[i] == delim)
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

	in[i] = '\0';
	out[count++] = in + start;
	return count;
}

void *log_handler(void *logArgs)
{
	log_queue *lq = ((struct log_args *)logArgs)->logs;

	char *log;
	char *recvStr = malloc(20);

	struct addrinfo serverSide,*logHostInfo[NUM_LOG];
	long int logSock[NUM_LOG];
	long int connected[NUM_LOG];
	long int currLOG = 0, i;

	for(i=0;i<NUM_LOG;i++) 
	{
		logSock[i] = 0;
		connected[i] = 0;
	}

	memset(&serverSide, 0, sizeof(serverSide) );
	serverSide.ai_family = AF_INET;
	serverSide.ai_socktype = SOCK_STREAM;

	log = ldeq(lq);
	
	while(1)
	{
		if(!connected[currLOG])
		{
			if(getaddrinfo(logServerHost[currLOG],logServerPort,&serverSide,&logHostInfo[currLOG]))
			{
				puts("Get addr error\n");
				currLOG = (currLOG + 1) % NUM_LOG;
				usleep(1);
				continue;
			}

			logSock[currLOG] = socket(logHostInfo[currLOG]->ai_family, logHostInfo[currLOG]->ai_socktype, logHostInfo[currLOG]->ai_protocol);
			if (logSock[currLOG] == -1)
			{
				puts("Could not create socket");
				usleep(1);
				continue;
			}

			if (connect(logSock[currLOG], logHostInfo[currLOG]->ai_addr, logHostInfo[currLOG]->ai_addrlen) < 0)
			{
				perror("connect failed. Error");
				close(logSock[currLOG]);
				usleep(1);
				continue;
			}
			connected[currLOG] = 1;
		}

		if( send(logSock[currLOG], log, strlen(log) + 1, 0) < 1)
		{
			puts("Send failed");
			connected[currLOG] = 0;
			close(logSock[currLOG]);
			continue;
		}

		if( recv(logSock[currLOG], recvStr, 20, 0) < 1)
		{
			puts("recv failed");
			connected[currLOG] = 0;
			close(logSock[currLOG]);
			continue;
		}
		free(log);
		currLOG = (currLOG + 1) % NUM_LOG;

		log = ldeq(lq);
	}

	return 0;
}

void lenq(log_queue *q, char *str, long int strsize)
{
	pthread_mutex_lock(q->busy);
	q->end->log = malloc(strsize);
	strcpy(q->end->log, str);
	q->end->next = calloc(1, sizeof(log_queue_element));
	q->end = q->end->next;
	sem_post(q->length);
	pthread_mutex_unlock(q->busy);
	return;
}

char *ldeq(log_queue *q)
{
	sem_wait(q->length);
	pthread_mutex_lock(q->busy);
	if(!q->start->log) return NULL;
	char *strlog = q->start->log;
	log_queue_element *element = q->start;
	q->start = q->start->next;
	free(element);
	pthread_mutex_unlock(q->busy);
	return strlog;
}

quote_queue_element *qenq(quote_queue *q, char *request, long int requestLen, char *transID, long int quoteAddress)
{
	pthread_mutex_lock(q->busy);
	q->end->request = request;
	q->end->requestLen = requestLen;
	q->end->transID = transID;
	q->end->quoteAddress = quoteAddress;
	if (sem_init(&q->end->returned, 0, 0))
	{
		perror("semaphore init failed");
		return NULL;
	}
	quote_queue_element *element = q->end;
	q->end->next = calloc(1, sizeof(quote_queue_element));
	q->end = q->end->next;
	sem_post(q->length);
	pthread_mutex_unlock(q->busy);
	return element;
}

quote_queue_element *qdeq(quote_queue *q)
{
	sem_wait(q->length);
	pthread_mutex_lock(q->busy);
	if(!q->start->request)
	{
		pthread_mutex_unlock(q->busy);
		return NULL;
	}
	quote_queue_element *element = q->start;
	q->start = q->start->next;
	pthread_mutex_unlock(q->busy);
	return element;
}

long int str2price(char *str)
{
	if(str == NULL) return -1;
	long int dollars = 0, cents = 0, i = 0, dec = 3, prclen;

	while(str[i] != ',') i++;
	prclen = i;

	if(
		prclen == 3 &&
		( (long int)str[0] > 48 || (long int)str[0] < 57 ) &&
		( (long int)str[2] > 48 || (long int)str[2] < 57 ) &&
		str[1] == '.'
	){
		dollars = (long int)str[0] - 48;
		cents = (long int)str[2] - 48;
		return dollars*100 + cents;
	}
	else if(prclen < 4) return -1;

	if(str[prclen-2] == '.') dec = 2;
	else if(str[prclen-3] != '.') return -1;	

	for(i=0;i<prclen-dec;i++)
	{
		if((long int)str[i] < 48 || (long int)str[i] > 57) return -1;
		dollars = 10*dollars + (long int)str[i] - 48;
	}
	i++;

	for(;i<prclen;i++)
	{
		if((long int)str[i] < 48 || (long int)str[i] > 57) return -1;
		cents = 10*cents + (long int)str[i] - 48;
	}

	return dollars*100 + cents;
}
