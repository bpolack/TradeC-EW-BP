#include <stdio.h>
#include <string.h>
#include<unistd.h>
#include<stdlib.h>
#include<netdb.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<semaphore.h>

//gcc -Wall Workload.c -o Workload -lpthread

#define NUM_THREADS 50
#define TRANS_NUM 5
#define USER_CHARS 62
#define USERID_MAX_CHARS 10

typedef struct comm_queue_element
{
	char *comm;
	long int commLen;
	struct comm_queue_element *next;
} comm_queue_element;

typedef struct comm_queue
{
	struct comm_queue_element *start;
	struct comm_queue_element *end;
	sem_t length;
	pthread_mutex_t busy;
} comm_queue;

struct comm_args
{
	struct comm_queue *commList;
	long int *transSocket;
	long int *init;
};

void *comm_handler(void *commArgs);

long int userSort(char *userID);
long int delimSepNoCpy(char *in, char **out, char delim);
comm_queue_element *cenq(comm_queue *q, char *comm, long int commLen);
comm_queue_element *cdeq(comm_queue *q);

const char *transServers[] =
{
	"b140.seng.uvic.ca",
	"b141.seng.uvic.ca",
	"b147.seng.uvic.ca",
	"b139.seng.uvic.ca",
	"b135.seng.uvic.ca",
	"b148.seng.uvic.ca"	
};/*
const char *transServers[] =
{
	"b147.seng.uvic.ca"
};*/
const char *port = "44422";
//const char *hostname = "localhost";

sem_t transCount;

int main(int argc, char ** argv){
	FILE *wl;
	char *op = malloc(100);
	char *commands = argv[1];
	puts(commands);
	
	if ((wl = fopen(commands, "r")) == NULL)
	{
		perror ("Error opening file");
		return 1;
	}


	if (sem_init(&transCount, 0, 0))
	{
		perror("mutex init failed");
		return 1;
	}

	char *message = malloc(500);
	char **messageSep = calloc(4,sizeof(char *));
	long int messageLen;
	long int i, j;
	char *userID, *userCmd;
	long int transID = 1;
	long int init;

	struct addrinfo serverSide,*serverInfo[TRANS_NUM];

	memset(&serverSide, 0, sizeof serverSide);
	serverSide.ai_family = AF_INET;
	serverSide.ai_socktype = SOCK_STREAM;

	for(i=0;i<TRANS_NUM;i++)
	{
		if(getaddrinfo(transServers[i],port,&serverSide,&serverInfo[i])){
			perror("Get addr error");
		}
	}
	
	long int transSocket[NUM_THREADS];
	comm_queue *cList[NUM_THREADS];
	pthread_t comm_thread[NUM_THREADS];
	for(i=0,j=0;i<NUM_THREADS;i++,j=(j+1)%TRANS_NUM)
	{
		transSocket[i] = socket(serverInfo[j]->ai_family, serverInfo[j]->ai_socktype, serverInfo[j]->ai_protocol);
		if (transSocket[i] == -1)
		{
			perror("Could not create socket");
			return 1;
		}

		if (connect(transSocket[i], serverInfo[j]->ai_addr, serverInfo[j]->ai_addrlen) < 0)
		{
			perror("connect failed. Error");
			return 1;
		}
		
		cList[i] = calloc(1, sizeof(comm_queue));
		cList[i]->start = calloc(1, sizeof(comm_queue_element));
		cList[i]->end = cList[i]->start;
		if (pthread_mutex_init(&cList[i]->busy, NULL))
		{
			perror("mutex init failed");
			free(cList[i]->start);
			free(cList[i]);
			return 1;
		}
		if (sem_init(&cList[i]->length, 0, 0))
		{
			perror("mutex init failed");
			free(cList[i]->start);
			free(cList[i]);
			return 1;
		}

		init = 0;
		struct comm_args commArgs = {cList[i], &transSocket[i], &init};
			 
		if( pthread_create( &comm_thread[i] , NULL ,  comm_handler , (void*) &commArgs) < 0)
		{
			perror("could not create thread");
			return 1;
		}
		while(!init) usleep(1);
	}

	long int userIntID;
	//keep communicating with server
	while(fgets(op , 100 , wl))
	{
		messageLen = strlen(op);

		if(op[messageLen - 1] == '\0') messageLen--;		
		if(op[messageLen - 1] == '\n') messageLen--;
		if(op[messageLen - 1] == ' ') messageLen--;
		op[messageLen++] = '\0';

		messageSep[3] = NULL;
		delimSepNoCpy(op, messageSep, ' ');
		delimSepNoCpy(messageSep[1], messageSep + 1, ',');

		userID = messageSep[2];
		userCmd = messageSep[1];
		userIntID = userSort(userID);

		if(messageSep[3]) messageLen = sprintf(message, "WSC|%ld|%s|%s,%s", transID, userCmd, userID, messageSep[3]) + 1;
		else messageLen = sprintf(message, "WSC|%ld|%s|%s", transID, userCmd, userID) + 1;
		cenq(cList[userIntID], message, messageLen);
		transID++;
	}
	
	fclose (wl);

	for(i=0;i<NUM_THREADS;i++)
	{
		cenq(cList[i], NULL, 0);
		pthread_join(comm_thread[i], NULL);
		close(transSocket[i]);
	}
	int semVal;
	sem_getvalue(&transCount, &semVal);
	printf("%d\n", semVal);
		
	return 0;
}

void *comm_handler(void *commArgs)
{
	comm_queue *cq = ((struct comm_args *)commArgs)->commList;
	long int *transSocket = ((struct comm_args *)commArgs)->transSocket;
	long int *init = ((struct comm_args *)commArgs)->init;
	
	long int readSize;
	char *commStr = NULL;
	long int commStrLen;
	char *message = malloc(5000);
	
	comm_queue_element *command;

	*init = 1;
	puts("Connected");
	
	while((command = cdeq(cq)))
	{
		commStr = command->comm;
		commStrLen = command->commLen;

		free(command);
		puts(commStr);

		if( send(*transSocket, commStr, commStrLen, 0) < 1)
		{
			perror("Send failed");
			pthread_exit(NULL);
		}
		free(commStr);
		sem_post(&transCount);

		//Receive a reply from the server
		if( (readSize = recv(*transSocket, message, 5000 , 0)) < 1)
		{
			perror("recv failed");
			pthread_exit(NULL);
		}
		//puts(message);
	}

	return 0;
}

long int userSort(char *userID)
{
	if(!userID) return -1;

	long int i, c, sum = 0, len = strlen(userID);
	for(i=0;i<len;i++)
	{
		c = (long int)userID[i];
		if( c > 47 && c < 58 ) sum += c - 48;
		else if( c > 96 && c < 123 ) sum += c - 87;
		else if( c > 64 && c < 91 ) sum += c - 29;
		else return 0;
	}
	return sum % NUM_THREADS;
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
			if(count == 2) break;
		}
	}
	in[i] = '\0';
	out[count++] = in + start;
	return count;
}

comm_queue_element *cenq(comm_queue *q, char *comm, long int commLen)
{
	pthread_mutex_lock(&q->busy);
	if(comm)
	{
		q->end->comm = malloc(commLen);
		strcpy(q->end->comm, comm);
	}
	q->end->commLen = commLen;
	comm_queue_element *element = q->end;
	q->end->next = calloc(1, sizeof(comm_queue_element));
	q->end = q->end->next;
	sem_post(&q->length);
	pthread_mutex_unlock(&q->busy);
	return element;
}

comm_queue_element *cdeq(comm_queue *q)
{
	sem_wait(&q->length);
	pthread_mutex_lock(&q->busy);
	if(!q->start->comm)
	{
		q->start = q->start->next;
		pthread_mutex_unlock(&q->busy);
		return NULL;
	}
	comm_queue_element *element = q->start;
	q->start = q->start->next;
	pthread_mutex_unlock(&q->busy);
	return element;
}
