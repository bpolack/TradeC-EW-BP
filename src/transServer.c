#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<netdb.h>
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
#include<sys/time.h>
#include<libpq-fe.h>
#include<semaphore.h>

//gcc -Wall transServer.c -o transServer -lpthread -I/seng/seng462/group2/seng462DB/include -L/seng/seng462/group2/seng462DB/lib -lpq -O2

#define USER_CHARS 62
#define STOCK_CHARS 26
#define USERID_MAX_CHARS 10
#define STOCKID_MAX_CHARS 3
#define NUM_DB 5
#define NUM_LB 1
#define NUM_LOG 1
#define quoteTo 28000

typedef struct user_stock_tree
{
	struct user_stock_tree *parent;
	struct user_stock_tree **child;
	struct user_stock *userStockObj;
} user_stock_tree;

typedef struct user_tree
{
	struct user_tree *parent;
	struct user_tree **child;
	struct user *userObj;
} user_tree;

typedef struct stock_tree
{
	struct stock_tree *parent;
	struct stock_tree **child;
	struct stock *stockObj;
} stock_tree;

typedef struct trigger_tree
{
	struct trigger_tree *parent;
	struct trigger_tree **child;
	struct trigger *triggerObj;
} trigger_tree;

typedef struct trigger
{
	char type;
	long int trig;
	long int amount;
	char *transID;
	struct trigger_tree *parent;
	struct user *userObj;
	struct stock *stockObj;
	struct trigger *next;
	struct trigger *prev;
} trigger;

typedef struct stock
{
	char *stockID;
	long int price;
	char *key;
	long long ts;
	pthread_mutex_t busy;
	struct stock_tree *parent;
	struct trigger *curr;
	struct stock *next;
	struct stock *prev;
} stock;

typedef struct stock_list
{
	struct stock *curr;
	pthread_mutex_t *busy;
} stock_list;

typedef struct user_list
{
	struct user *curr;
	pthread_mutex_t *busy;
	long int num;
	sem_t *avail;
} user_list;

typedef struct user_stock
{
	long int num;
	struct user_stock_tree *parent;
	struct stock *stockObj;
} user_stock;

typedef struct user
{
	char *userID;
	struct user_tree *parent;
	long int balance;
	long long prompt_time;
	struct stock *stockObj;
	long int reserve;
	long int no;
	long int buysell;
	long int dbID;
	pthread_mutex_t busy;
	struct comm_queue *commList;
	struct trigger_tree *tTree;
	struct user_stock_tree *usTree;
	struct user *next;
	struct user *prev;
} user;

typedef struct comm_queue_element
{
	char *comm;
	long int *webServerSock;
	char *transID;
	char *userCmd;
	struct comm_queue_element *next;
} comm_queue_element;

typedef struct comm_queue
{
	struct comm_queue_element *start;
	struct comm_queue_element *end;
	long int length;
	pthread_mutex_t busy;
} comm_queue;

typedef struct log_queue_element
{
	char *log;
	struct log_queue_element *next;
} log_queue_element;

typedef struct log_queue
{
	struct log_queue_element *start;
	struct log_queue_element *end;
	sem_t *length;
	pthread_mutex_t *busy;
} log_queue;

typedef struct db_queue_element
{
	char *request;
	char *transID;
	char *userCmd;
	long int dbID;
	sem_t *srcReturned;
	sem_t *destReturned;
	PGresult *result;
	struct db_queue_element *next;
} db_queue_element;

typedef struct db_queue
{
	struct db_queue_element *start;
	struct db_queue_element *end;
	sem_t *length;
	pthread_mutex_t *busy;
} db_queue;

typedef struct quote_queue_element
{
	struct stock *stockObj;
	char *userID;
	char *transID;
	char *userCmd;
	sem_t *returned;
	struct quote_queue_element *next;
} quote_queue_element;

typedef struct quote_queue
{
	struct quote_queue_element *start;
	struct quote_queue_element *end;
	sem_t *length;
	pthread_mutex_t *busy;
} quote_queue;

struct conn_args
{
	struct db_queue *dbReads;
	struct db_queue *dbWrites;
	struct log_queue *logs;
	struct user_tree *users;
	pthread_mutex_t *uTreeBusy;
	struct user_list *usersList;
	struct stock_list *stocksList;
	struct stock_tree *stocks;
	pthread_mutex_t *sTreeBusy;
	long int *clientSock;
	long int *init;
};

struct comm_args
{
	struct db_queue *dbWrites;
	struct db_queue *dbReads;
	struct log_queue *logs;
	struct quote_queue *quotes;
	struct stock_list *stocksList;
	struct stock_tree *stocks;
	pthread_mutex_t *sTreeBusy;
	struct user_list *usersList;
};

struct trig_args
{
	struct log_queue *logs;
	struct quote_queue *quotes;
	struct db_queue *dbWrites;
	struct stock_list *stocksList;
};

struct log_args
{
	struct log_queue *logs;
};

struct dbWrite_args
{
	struct db_queue *dbWrites;
	struct log_queue *logs;
};

struct dbRead_args
{
	struct db_queue *dbReads;
	struct log_queue *logs;
};

struct quote_args
{
	struct quote_queue *quotes;
	struct log_queue *logs;
};

//the thread function
void *connection_handler(void *);
void *trigger_handler(void *);
void *command_handler(void *);
void *log_handler(void *);
void *dbWrite_handler(void *dbWriteArgs);
void *dbRead_handler(void *dbReadArgs);
void *quote_handler(void *quoteArgs);
long long getmTS();
char *price2str(char *str, long int prc);
long int str2price(char *prc);
long int commaSep(char *in, char **out);
long int delimSepNoCpy(char *in, char **out, char delim);
long int delimSepMaxDelim(char *in, char **out, char delim, char maxDelim);
comm_queue_element *cenq(comm_queue *cq, char *transID, char *userCmd, char *comm, long int *sock, user_list *ul);
comm_queue_element *cdeq(comm_queue *cq);
char *quickSend(long int *sock, char *str, long int sendLen);
void lenq(log_queue *lq, char *str, long int strsize);
char *ldeq(log_queue *lq);
db_queue_element *dbdeq(db_queue *q);
db_queue_element *dbenq(db_queue *q, char *request, char *transID, char *userCmd, sem_t *returned, long int dbID);
quote_queue_element *qenq(quote_queue *q, stock *stockObj, char *userID, char *transID, char *userCmd, sem_t *returned);
quote_queue_element *qdeq(quote_queue *q);

void removeUserStock(user_stock *userStockObj);
user_stock *getUserStock(user *userObj, stock *stockObj);
user_stock *addUserStock(user *userObj, stock *stockObj, long int numStock);
void removeTrigger(trigger *trigObj);
trigger *getTrigger(user *userObj, stock *stockObj, char type);
trigger *addTrigger(user *userObj, char *transID, stock *stockObj, char type, long int amount, long int trigPrc);
stock *addStock(stock_tree *st, pthread_mutex_t *sTreeBusy, stock_list *sl, char *stockID);
stock *getStock(stock_tree *st, pthread_mutex_t *sTreeBusy, char *stockID);
user *addUser(user_tree *ut, pthread_mutex_t *uTreeBusy, user_list *ul, char *userID, stock_tree *sTree, pthread_mutex_t *sTreeBusy, stock_list *sList, db_queue *dbRq, char *transID, char *userCmd, sem_t *returned);
user *getUser(user_tree *ut, pthread_mutex_t *uTreeBusy, char *userID);
void removeUser(user *userObj, user_list *ul);
long int getAllTriggers(trigger **triggers, trigger_tree *tTree, long int count);
long int getAllUserStocks(user_stock **userStocks, user_stock_tree *usTree, long int count);
char *validUserID(char *userID);
char *validStockID(char *stockID);
long int userSort(char *userID);

const char* quoteRequest = "QCR|%s|%s|%s,%s\n";
const char* accTransLog = "accTrans,%lld,%s,%s,%s,%s,%s";
const char* userCmdLog = "userCmd,%lld,%s,%s,%s,%s,%s,%s,%s";
const char* serverName = "TRAN1";
const char* spc = " ";
const char* exec_succ = "Executed [%s] successfully. %s";
const char* exec_fail = "Execution of [%s] failed.";

const char* insertAcct = "INSERT INTO accounts (User_ID,Balance) SELECT '%s', %s WHERE NOT EXISTS (SELECT * FROM accounts WHERE User_ID = '%s');";
const char* updateBal = "UPDATE accounts SET balance = '%s' WHERE user_id = '%s';";

const char* insertStk = "INSERT INTO stocks (User_ID,Stock_ID,Amount) VALUES ('%s', '%s', %ld);";
const char* updateStk = "UPDATE stocks SET amount = %ld WHERE user_id = '%s' AND stock_id = '%s';";

const char* insertTran = "INSERT INTO transactions (User_ID,Trans_ID,Stock_ID,Adjustment,Timestamp,Command) VALUES ('%s', '%s', '%s', %s, %lld, '%c');";

const char* insertTrig = "INSERT INTO triggers (User_ID,Trans_ID,Stock_ID,Trigger_Type,Reserved_Funds,Trigger_Value) VALUES ('%s', '%s', '%s', '%c', %s, %s);";
const char* updateTrig = "UPDATE triggers SET trigger_value = %s WHERE user_id = '%s' AND stock_id = '%s' AND Trigger_Type = '%c';";
const char* deleteTrig = "DELETE FROM triggers WHERE User_ID = '%s' AND Stock_ID = '%s' AND Trigger_Type = '%c';";

const char* selectUserBal = "SELECT balance FROM accounts WHERE User_ID = '%s';";
const char* selectUserStock = "SELECT * FROM stocks WHERE User_ID = '%s';";
const char* selectUserTrans = "SELECT * FROM transactions WHERE User_ID = '%s';";
const char* selectUserTrig = "SELECT * FROM triggers WHERE User_ID = '%s';";

const char *loadBalPort = "44421";
const char *logServerPort = "44420";
const char *logServerHost[] = 
{
	"b145.seng.uvic.ca",
	"b146.seng.uvic.ca"
};
const char *loadBalHost[] = 
{
	"b146.seng.uvic.ca",
	"b148.seng.uvic.ca",
	"b142.seng.uvic.ca"
};
const char *dbConnInfo[] = 
{
	//"host=b140.seng.uvic.ca port=44427 dbname=group2 user=seng462"
	//"host=b140.seng.uvic.ca port=44427 dbname=trading user=group2",
	"host=b141.seng.uvic.ca port=44425 dbname=trading",
	"host=b140.seng.uvic.ca port=44425 dbname=trading",
	"host=b147.seng.uvic.ca port=44425 dbname=trading",
	"host=b139.seng.uvic.ca port=44425 dbname=trading",
	"host=b135.seng.uvic.ca port=44425 dbname=trading"
};
 
int main( int argc , char *argv[])
{
	log_queue *logList = calloc(1, sizeof(log_queue));
	logList->start = calloc(1, sizeof(log_queue_element));
	logList->end = logList->start;
	logList->busy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(logList->busy, NULL))
	{
		printf("\n mutex init failed\n");
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
		printf("mutex init failed");
		return 1;
	}
	quoteList->length = malloc(sizeof(sem_t));
	if (sem_init(quoteList->length, 0, 0))
	{
		perror("semaphore init failed");
		return 1;
	}

	db_queue *dbWriteList = calloc(1, sizeof(db_queue));
	dbWriteList->start = calloc(1, sizeof(db_queue_element));
	dbWriteList->end = dbWriteList->start;
	dbWriteList->busy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(dbWriteList->busy, NULL))
	{
		printf("mutex init failed");
		return 1;
	}
	dbWriteList->length = malloc(sizeof(sem_t));
	if (sem_init(dbWriteList->length, 0, 0))
	{
		perror("semaphore init failed");
		return 1;
	}
	
	db_queue *dbReadList = calloc(1, sizeof(db_queue));
	dbReadList->start = calloc(1, sizeof(db_queue_element));
	dbReadList->end = dbReadList->start;
	dbReadList->busy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(dbReadList->busy, NULL))
	{
		printf("mutex init failed");
		return 1;
	}
	dbReadList->length = malloc(sizeof(sem_t));
	if (sem_init(dbReadList->length, 0, 0))
	{
		perror("semaphore init failed");
		return 1;
	}

	user_tree *uTree = calloc(1, sizeof(user_tree));
	pthread_mutex_t *uTreeBusy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(uTreeBusy, NULL))
	{
		printf("mutex init failed");
		return 1;
	}

	user_list *uList = calloc(1, sizeof(user_list));
	uList->busy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(uList->busy, NULL))
	{
		printf("mutex init failed");
		return 1;
	}
	uList->avail = malloc(sizeof(sem_t));
	if (sem_init(uList->avail, 0, 0))
	{
		perror("semaphore init failed");
		return 1;
	}

	stock_tree *sTree = calloc(1, sizeof(stock_tree));
	pthread_mutex_t *sTreeBusy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(sTreeBusy, NULL))
	{
		printf("mutex init failed");
		return 1;
	}

	stock_list *sList = calloc(1, sizeof(stock_list));
	sList->busy = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(sList->busy, NULL))
	{
		printf("mutex init failed");
		return 1;
	}

	PGconn *db[NUM_DB];
	long int currDB;
	for(currDB=0;currDB<NUM_DB;currDB++)
	{
		db[currDB] = PQconnectdb(dbConnInfo[currDB]);
		if(PQstatus(db[currDB]) != CONNECTION_OK)
		{
			printf("%ld\n", currDB);
			return 1;
		}
		puts("connected to db");
		PQexec(db[currDB], "DELETE FROM stocks");
		PQexec(db[currDB], "DELETE FROM triggers");
		PQexec(db[currDB], "DELETE FROM transactions");
		PQexec(db[currDB], "DELETE FROM accounts");
		PQfinish(db[currDB]);
	}

/*******************************************************************/
//Client setup
/*******************************************************************/
	long int i, init;

	struct comm_args commArgs = {dbWriteList, dbReadList, logList, quoteList, sList, sTree, sTreeBusy, uList};
	for(i=0;i<30;i++)
	{
		pthread_t command_thread;
		if( pthread_create( &command_thread , NULL ,  command_handler , (void*) &commArgs) < 0)
		{
			perror("could not create thread");
			return 1;
		}
	}

	struct trig_args trigArgs = {logList, quoteList, dbWriteList, sList};
	for(i=0;i<1;i++)
	{
		pthread_t trigger_thread;
		if( pthread_create( &trigger_thread , NULL ,  trigger_handler , (void*) &trigArgs) < 0)
		{
			perror("could not create thread");
			return 1;
		}
	}
	
	struct quote_args quoteArgs = {quoteList, logList};
	for(i=0;i<NUM_LB*2;i++)
	{
		pthread_t quote_thread;
		if( pthread_create( &quote_thread , NULL ,  quote_handler , (void*) &quoteArgs) < 0)
		{
			perror("could not create thread");
			return 1;
		}
	}

	struct log_args logArgs = {logList};
	for(i=0;i<1;i++)
	{
		pthread_t log_thread;
		if( pthread_create( &log_thread , NULL ,  log_handler , (void*) &logArgs) < 0)
		{
			perror("could not create thread");
			return 1;
		}
	}

	struct dbWrite_args dbWriteArgs = {dbWriteList, logList};
	for(i=0;i<NUM_DB;i++)
	{
		pthread_t dbWrite_thread;
		if( pthread_create( &dbWrite_thread , NULL ,  dbWrite_handler , (void*) &dbWriteArgs) < 0)
		{
			perror("could not create thread");
			return 1;
		}
	}
	
	struct dbRead_args dbReadArgs = {dbReadList, logList};
	for(i=0;i<1;i++)
	{
		pthread_t dbRead_thread;
		if( pthread_create( &dbRead_thread , NULL ,  dbRead_handler , (void*) &dbReadArgs) < 0)
		{
			perror("could not create thread");
			return 1;
		}
	}

/*******************************************************************/
//Server setup
/*******************************************************************/
	long int socketDesc , clientSock , c , *webSock;
	struct sockaddr_in transServer;
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
	transServer.sin_family = AF_INET;
	transServer.sin_addr.s_addr = INADDR_ANY;
	transServer.sin_port = htons( 44422 );

	//Bind
	if( bind(socketDesc,(struct sockaddr *)&transServer , sizeof(struct sockaddr)) < 0)
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
	c = sizeof(struct sockaddr);

	while( (clientSock = accept(socketDesc, NULL, (socklen_t*)&c)) )
	{
		init = 0;
		puts("Connection accepted");
		//setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&to, sizeof(struct timeval));
		
		pthread_t sniffer_thread;
		webSock = malloc(sizeof(long int));
		*webSock = clientSock;

		struct conn_args conns = {dbReadList, dbWriteList, logList, uTree, uTreeBusy, uList, sList, sTree, sTreeBusy, webSock, &init};
		 
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
 
/*********************************************************************************************/
//This will handle connection for each client
/*********************************************************************************************/
void *connection_handler(void *connArgs)
{
	//Get the socket descriptor
	long int *sock = ((struct conn_args *)connArgs)->clientSock;
	long int *init = ((struct conn_args *)connArgs)->init;
	db_queue *dbRq = ((struct conn_args *)connArgs)->dbReads;
	db_queue *dbWq = ((struct conn_args *)connArgs)->dbWrites;
	log_queue *lq = ((struct conn_args *)connArgs)->logs;
	user_tree *uTree = ((struct conn_args *)connArgs)->users;
	pthread_mutex_t *uTreeBusy = ((struct conn_args *)connArgs)->uTreeBusy;
	user_list *uList = ((struct conn_args *)connArgs)->usersList;
	stock_list *sList = ((struct conn_args *)connArgs)->stocksList;
	stock_tree *sTree = ((struct conn_args *)connArgs)->stocks;
	pthread_mutex_t *sTreeBusy = ((struct conn_args *)connArgs)->sTreeBusy;
	
	sem_t returned;
	if (sem_init(&returned, 0, 0))
	{
		perror("semaphore init failed");
		pthread_exit(NULL);
	}

	char *received = "REC";
	long int readSize;
	long int messageLen;
	char *clientMessage = malloc(200);
	char **message = calloc(5, sizeof(void *));
	
	char *userCmd;
	char *transID;
	char *userID;
	char *file;
	user *cUser;
	db_queue_element *dbResp;

	char *dbStr = malloc(5000);
	char *logStr = malloc(500);
	char *responseStr = malloc(500);
	long int logStrLen;
	long int responseStrLen;

	*init = 1;
	
	puts("Connection Initialized");
	//Receive a message from client
	while( (readSize = recv(*sock, clientMessage, 200, 0)) > 0 )
	{
		if(clientMessage[readSize - 1] == '\0') readSize--;		
		if(clientMessage[readSize - 1] == '\n') readSize--;
		if(clientMessage[readSize - 1] == '\r') readSize--;
		if(clientMessage[readSize - 1] == ' ') readSize--;
		clientMessage[readSize] = '\0';

		//Separate the client message into array separated by the '|' delimeter.
		//Message structure is expected to be: 'InternalCmd|TransNum|UserCmd|Data'.
		message[4] = NULL;
		messageLen = delimSepNoCpy(clientMessage, message, '|');

		quickSend(sock, received, 4);

		if (messageLen != 4 && messageLen != 2)
		{
			puts("Invalid message length. Disregarding command.");
			continue;
		}

		else if(!strcmp("USR", message[0]))
		{
			userID = message[1];
			if(!(cUser = getUser(uTree, uTreeBusy, userID) ) )
			{
				responseStrLen = sprintf(responseStr, "NOT|%ld", uList->num);
				quickSend(sock, responseStr, responseStrLen + 1);
			}
			else
			{
				responseStrLen = sprintf(responseStr, "HAV|%s", userID);
				quickSend(sock, responseStr, responseStrLen + 1);
			}
		}

		//Respond immediately if Internal Command = 'WLC" Workload Generator Command.
		else if (!strcmp("WSC", message[0]))
		{
			delimSepMaxDelim(message[3], message + 3, ',', 1);
			transID = message[1];
			userCmd = message[2];
			file = message[3];
			if(!strcmp("DUMPLOG", userCmd) && (file[0] == '.' || file[0] == '/') )
			{
				logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,spc,spc,file,spc);
				lenq(lq, logStr, logStrLen + 1);
				
				logStrLen = sprintf(logStr, "%s,%s", userCmd, file);
				lenq(lq, logStr, logStrLen + 1);
				//lenq(lq, logStr, logStrLen + 1);

				//DUMPLOG Successful.
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
				quickSend(sock, responseStr, responseStrLen + 1);
			}
			else
			{
				if(!validUserID(userID = message[3]))
				{
					responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -18);
					quickSend(sock, responseStr, responseStrLen + 1);
					continue;
				}
				
				if ( !(cUser = getUser(uTree, uTreeBusy, userID) ) )
				{
					cUser = addUser(uTree, uTreeBusy, uList, userID, sTree, sTreeBusy, sList, dbRq, transID, userCmd, &returned);
					//Create new user with the amount specified in the command.
					sprintf(dbStr, insertAcct, userID, "0.00", userID);
					dbResp = dbenq(dbWq, dbStr, transID, userCmd, &returned, cUser->dbID);
					sem_wait(dbResp->destReturned);
					sem_post(dbResp->srcReturned);
				}
				cenq(cUser->commList, transID, userCmd, message[4], sock, uList);
			}
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
	
	sem_destroy(&returned);
	//Free the socket pointer
	close(*sock);
	//free(sock);
	free(clientMessage);
	free(message);
	free(dbStr);
	free(logStr);
	free(responseStr);

	return 0;
}

/*********************************************************************************************/
//This will parse through the command queue
/*********************************************************************************************/
void *command_handler(void *commArgs)
{
	db_queue *dbWq = ((struct comm_args *)commArgs)->dbWrites;
	//db_queue *dbRq = ((struct comm_args *)commArgs)->dbReads;
	log_queue *lq = ((struct comm_args *)commArgs)->logs;
	quote_queue *qq = ((struct comm_args *)commArgs)->quotes;
	stock_list *sList = ((struct comm_args *)commArgs)->stocksList;
	stock_tree *sTree = ((struct comm_args *)commArgs)->stocks;
	pthread_mutex_t *sTreeBusy = ((struct comm_args *)commArgs)->sTreeBusy;
	user_list *uList = ((struct comm_args *)commArgs)->usersList;

	sem_t *returned = malloc(sizeof(sem_t));
	if (sem_init(returned, 0, 0))
	{
		perror("semaphore init failed");
		pthread_exit(NULL);
	}

	comm_queue_element *command;
	char *commStr = NULL;
	char **comm = calloc(2, sizeof(char *));
	
	quote_queue_element *quoteResp;
	long int quotePrice;

	char *userCmd = NULL;
	long int commLen;
	char *transID = NULL;
	char *userID;
	char *stockID;
	long int prc;
	char *file;
	char *cprc;
	long int *webSock;

	char *strPrcMod = malloc(20);
	char *strPrcNew = malloc(20);
	char *responseStr = malloc(0x100000);
	char *tmpStr;
	char *dbStr = malloc(5000);
	char *logStr = malloc(500);
	long int responseStrLen;
	long int logStrLen;

	long int i,numTriggers, numUserStocks;
	char *strPos, *userStockInfo, *triggerInfo;//, *transInfo;
	userStockInfo = malloc(0x10000);
	triggerInfo = malloc(0x10000);

	user *cUser;
	stock *cStock;
	trigger *cTrigger;
	user_stock *cUserStock;
	db_queue_element *dbResp;
	
	puts("Command Handler Init");

	sem_wait(uList->avail);
	cUser = uList->curr;

	while(pthread_mutex_trylock(&cUser->busy))
	{
		pthread_mutex_lock(uList->busy);
		cUser = cUser->next;
		pthread_mutex_unlock(uList->busy);
	}
	sem_post(uList->avail);

	while(1)
	{
		pthread_mutex_unlock(&cUser->busy);
		pthread_mutex_lock(uList->busy);
		cUser = cUser->next;
		pthread_mutex_unlock(uList->busy);

		free(commStr);
		free(transID);
		free(userCmd);

		comm[0] = NULL;
		comm[1] = NULL;

		sem_wait(uList->avail);

		while(1)
		{
			if(pthread_mutex_trylock(&cUser->busy))
			{
				pthread_mutex_lock(uList->busy);
				cUser = cUser->next;
				pthread_mutex_unlock(uList->busy);
			}
			else
			{
				if(!cUser->commList->length)
				{
					pthread_mutex_unlock(&cUser->busy);
					pthread_mutex_lock(uList->busy);
					cUser = cUser->next;
					pthread_mutex_unlock(uList->busy);
				}
				else break;
			}
		}
		command = cdeq(cUser->commList);

		//Get the web server socket associated with the command.
		commStr = command->comm;
		webSock = command->webServerSock;
		transID = command->transID;
		userCmd = command->userCmd;
		userID = cUser->userID;

		free(command);

printf("%s	%s	%s	%s\n", transID, userCmd, userID, commStr);

		//Get the string for the user command and separate by commas into array comm[].
		commLen = delimSepNoCpy(commStr, comm, ',');

		tmpStr = dbStr;
/*********************************************************************************************/
//Add
/*********************************************************************************************/
		if(!strcmp("ADD", userCmd) && commLen == 1)
		{
			if( (prc = str2price(comm[0])) == -1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -19);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			cprc = comm[0];

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,spc,spc,cprc);
			lenq(lq, logStr, logStrLen + 1);

			cUser->balance = cUser->balance + prc;
			strPrcNew = price2str(strPrcNew, cUser->balance);
			//Update users current balance by the amount specified in the command.
			tmpStr += sprintf(tmpStr, updateBal, strPrcNew, userID);

			logStrLen = sprintf(logStr, accTransLog, getmTS(), serverName,transID,"add",userID,cprc);
			lenq(lq, logStr, logStrLen + 1);

			tmpStr += sprintf(tmpStr, insertTran, userID, transID, "...", cprc, getmTS(), 'a');

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);
			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);

			//Create success message and respond to client.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Quote
/*********************************************************************************************/
		else if(!strcmp("QUOTE", userCmd) && commLen == 1)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName, transID, userCmd, userID, stockID, spc, spc);
			lenq(lq, logStr, logStrLen + 1);

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if(!cStock->ts)
			{
				quoteResp = qenq(qq, cStock, userID, transID, userCmd, returned);
				sem_wait(quoteResp->returned);
			}

			//Create success message and respond to client with price and timestamp.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%ld,%lld", transID, userCmd, "SUCC", cStock->price, cStock->ts);
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Buy
/*********************************************************************************************/
		else if(!strcmp("BUY", userCmd) && commLen == 2)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			if( (prc = str2price(comm[1])) == -1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -19);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			cprc = comm[1];

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,stockID,spc,cprc);
			lenq(lq, logStr, logStrLen + 1);

			if(cUser->buysell)
			{
				//Buy Failed, "You already have a pending buy/sell."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -10);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if ( cUser->balance < prc )
			{
				//Buy Failed, "Not enough money in user account."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -14);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if((getmTS() - cStock->ts) > quoteTo)
			{
				quoteResp = qenq(qq, cStock, userID, transID, userCmd, returned);
				sem_wait(quoteResp->returned);
			}
			quotePrice = cStock->price;

			if(quotePrice < 1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -21);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;			
			}

			long int numStock = prc/quotePrice;

			if(!numStock)
			{
				//Buy Failed, "Not enough money entered, price is $$$ per stock (Valid for XX Seconds)."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d,%ld,%lld", transID, userCmd, "FAIL", -11, cStock->price, cStock->ts);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			long int buy = quotePrice * numStock;

			cUser->prompt_time = cStock->ts;
			cUser->stockObj = cStock;
			cUser->reserve = buy;
			cUser->no = numStock;
			cUser->buysell = 1;

			//Buy Cmd Success, "You have enough money to buy # shares of <stock> @ $$$ each. You have __ seconds to commit."
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%ld,%ld,%lld", transID, userCmd, "SUCC", numStock, cStock->price, cStock->ts);
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Commit Buy
/*********************************************************************************************/
		else if(!strcmp("COMMIT_BUY", userCmd) && commLen == 0)
		{
			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,spc,spc,spc);
			lenq(lq, logStr, logStrLen + 1);

			if(cUser->buysell != 1)
			{
				//Buy Failed, "No pending buy command."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -12);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if( ( getmTS() - cUser->prompt_time) > quoteTo)
			{
				cUser->prompt_time = 0;
				cUser->reserve = 0;
				cUser->no = 0;
				cUser->buysell = 0;
				cUser->stockObj = NULL;

				//Buy Failed, "Quote expired."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -13);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			strPrcMod = price2str(strPrcMod, cUser->reserve);
			cUser->balance = cUser->balance - cUser->reserve;
			strPrcNew = price2str(strPrcNew, cUser->balance);

			cStock = cUser->stockObj;
			stockID = cStock->stockID;

			cUserStock = getUserStock(cUser, cStock);

			if ( !cUserStock )
			{
				cUserStock = addUserStock(cUser, cStock, cUser->no);
				tmpStr += sprintf(tmpStr, insertStk, userID, stockID, cUserStock->num);
			}
			else
			{
				cUserStock->num = cUserStock->num + cUser->no;
				tmpStr += sprintf(tmpStr, updateStk, cUserStock->num, userID, stockID);
			}

			tmpStr += sprintf(tmpStr, updateBal, strPrcNew, userID);

			logStrLen = sprintf(logStr, accTransLog, getmTS(), serverName,transID,"remove",userID,strPrcMod);
			lenq(lq, logStr, logStrLen + 1);

			tmpStr += sprintf(tmpStr, insertTran, userID, transID, stockID, strPrcMod, getmTS(), 'b');

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);
			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);

			cUser->prompt_time = 0;
			cUser->reserve = 0;
			cUser->no = 0;
			cUser->buysell = 0;
			cUser->stockObj = NULL;

			//Commit Buy Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Cancel Buy
/*********************************************************************************************/
		else if(!strcmp("CANCEL_BUY", userCmd) && commLen == 0)
		{
			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,spc,spc,spc);
			lenq(lq, logStr, logStrLen + 1);

			if(cUser->buysell != 1)
			{
				//Cancel Buy Failed, "No pending buy command."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -12);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			cUser->prompt_time = 0;
			cUser->reserve = 0;
			cUser->no = 0;
			cUser->buysell = 0;
			cUser->stockObj = NULL;

			//Cancel Buy Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);	
		}
/*********************************************************************************************/
//Sell
/*********************************************************************************************/
		else if(!strcmp("SELL", userCmd) && commLen == 2)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if( (prc = str2price(comm[1])) == -1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -19);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			cprc = comm[1];

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,stockID,spc,cprc);
			lenq(lq, logStr, logStrLen + 1);

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if(!(cUserStock = getUserStock(cUser, cStock) ) || !cUserStock->num)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -15);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if(cUser->buysell)
			{
				//Sell Failed, "You already have a pending buy/sell."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -10);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if((getmTS() - cStock->ts) > quoteTo)
			{
				quoteResp = qenq(qq, cStock, userID, transID, userCmd, returned);
				sem_wait(quoteResp->returned);
			}
			quotePrice = cStock->price;

			if(quotePrice < 1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -21);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;			
			}
			long int numStock = prc/quotePrice;

			numStock++;

			if(cUserStock->num < numStock) numStock = cUserStock->num;

			long int sell = quotePrice*numStock;

			cUser->prompt_time = cStock->ts;
			cUser->stockObj = cStock;
			cUser->reserve = sell;
			cUser->no = numStock;
			cUser->buysell = -1;

			//Sell Cmd Success, "Enough money entered to sell # shares of <stock> @ $$$ each. You have __ seconds to commit."
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%ld,%ld,%lld", transID, userCmd, "SUCC", numStock, cStock->price, cStock->ts);
			quickSend(webSock, responseStr, responseStrLen + 1);			
		}
/*********************************************************************************************/
//Commit Sell
/*********************************************************************************************/
		else if(!strcmp("COMMIT_SELL", userCmd) && commLen == 0)
		{
			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,spc,spc,spc);
			lenq(lq, logStr, logStrLen + 1);

			if(cUser->buysell != -1)
			{
				//Sell Failed, "No pending sell command."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -12);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if( ( getmTS() - cUser->prompt_time) > quoteTo)
			{
				cUser->prompt_time = 0;
				cUser->reserve = 0;
				cUser->no = 0;
				cUser->buysell = 0;
				cUser->stockObj = NULL;

				//Sell Failed, "Quote expired."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -13);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			cStock = cUser->stockObj;
			stockID = cStock->stockID;

			if(!(cUserStock = getUserStock(cUser, cStock) ))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -15);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			strPrcMod = price2str(strPrcMod, cUser->reserve);
			cUser->balance = cUser->balance + cUser->reserve;
			strPrcNew = price2str(strPrcNew, cUser->balance);

			tmpStr += sprintf(tmpStr, updateBal, strPrcNew, userID);

			logStrLen = sprintf(logStr, accTransLog, getmTS(), serverName,transID,"add",userID,strPrcMod);
			lenq(lq, logStr, logStrLen + 1);
if(cUserStock->num < cUser->no) puts(transID);
			cUserStock->num = cUserStock->num - cUser->no;

			tmpStr += sprintf(tmpStr, updateStk, cUserStock->num, userID, stockID);

			tmpStr += sprintf(tmpStr, insertTran, userID, transID, stockID, strPrcMod, getmTS(), 's');

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);
			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);

			cUser->prompt_time = 0;
			cUser->reserve = 0;
			cUser->no = 0;
			cUser->buysell = 0;			
			cUser->stockObj = NULL;

			//Commit Sell Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Cancel Sell
/*********************************************************************************************/
		else if(!strcmp("CANCEL_SELL", userCmd) && commLen == 0)
		{
			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,spc,spc,spc);
			lenq(lq, logStr, logStrLen + 1);

			if(cUser->buysell != -1)
			{
				//Cancel Sell Failed, "No pending sell command."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -12);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			cUser->prompt_time = 0;
			cUser->reserve = 0;
			cUser->no = 0;
			cUser->buysell = 0;
			cUser->stockObj = NULL;

			//Cancel Sell Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Set Buy Amount
/*********************************************************************************************/
		else if(!strcmp("SET_BUY_AMOUNT", userCmd) && commLen == 2)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			if( (prc = str2price(comm[1])) == -1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -19);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			cprc = comm[1];

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,stockID,spc,cprc);
			lenq(lq, logStr, logStrLen + 1);

			if(cUser->buysell)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -10);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if( cUser->balance < prc )
			{
				//Failed, "Not enough money in user account."
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -14);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if( (cTrigger = getTrigger(cUser, cStock, 'b') ))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -16);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			cUser->balance = cUser->balance - prc;
			strPrcNew = price2str(strPrcNew, cUser->balance);

			tmpStr += sprintf(tmpStr, updateBal, strPrcNew, userID);

			logStrLen = sprintf(logStr, accTransLog, getmTS(), serverName,transID,"remove",userID,cprc);
			lenq(lq, logStr, logStrLen + 1);

			cTrigger = addTrigger(cUser, transID, cStock, 'b', prc, 0);

			tmpStr += sprintf(tmpStr, insertTrig, userID, transID, stockID, 'b', cprc, "0.00");

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);

			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);
			//Subtract from balance.

			//Set Buy Amount Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Cancel Set Buy
/*********************************************************************************************/
		else if(!strcmp("CANCEL_SET_BUY", userCmd) && commLen == 1)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,stockID,spc,spc);
			lenq(lq, logStr, logStrLen + 1);

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if(!(cTrigger = getTrigger(cUser, cStock, 'b') ))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -22);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			strPrcMod = price2str(strPrcMod, cTrigger->amount);
			cUser->balance = cUser->balance + cTrigger->amount;
			strPrcNew = price2str(strPrcNew, cUser->balance);
			removeTrigger(cTrigger);

			tmpStr += sprintf(tmpStr, updateBal, strPrcNew, userID);

			logStrLen = sprintf(logStr, accTransLog, getmTS(), serverName,transID,"add",userID,strPrcMod);
			lenq(lq, logStr, logStrLen + 1);

			tmpStr += sprintf(tmpStr, deleteTrig, userID, stockID, 'b');

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);
			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);

			//Cancel Set Buy Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Set Buy Trigger
/*********************************************************************************************/
		else if(!strcmp("SET_BUY_TRIGGER", userCmd) && commLen == 2)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			if( (prc = str2price(comm[1])) == -1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -19);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			cprc = comm[1];

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,stockID,spc,cprc);
			lenq(lq, logStr, logStrLen + 1);

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if(!(cTrigger = getTrigger(cUser, cStock, 'b') ))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -22);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			cTrigger->trig = prc;

			tmpStr += sprintf(tmpStr, updateTrig, cprc, userID, stockID, 'b');

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);
			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);

			//Set Buy Trigger Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Set Sell Amount
/*********************************************************************************************/
		else if(!strcmp("SET_SELL_AMOUNT", userCmd) && commLen == 2)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			if( (prc = str2price(comm[1])) == -1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -19);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			cprc = comm[1];

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,stockID,spc,cprc);
			lenq(lq, logStr, logStrLen + 1);

			if(cUser->buysell)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -10);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if(!(cUserStock = getUserStock(cUser, cStock) ))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -15);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			if( (cTrigger = getTrigger(cUser, cStock, 's') ))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -16);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			cTrigger = addTrigger(cUser, transID, cStock, 's', prc, 0);

			tmpStr += sprintf(tmpStr, insertTrig, userID, transID, stockID, 's', cprc, "0.00");

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);
			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);

			//Set Sell Amount Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Set Sell Trigger
/*********************************************************************************************/
		else if(!strcmp("SET_SELL_TRIGGER", userCmd) && commLen == 2)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			if( (prc = str2price(comm[1])) == -1)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -19);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			cprc = comm[1];

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,stockID,spc,cprc);
			lenq(lq, logStr, logStrLen + 1);

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if(!(cTrigger = getTrigger(cUser, cStock, 's') ))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -22);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			cTrigger->trig = prc;

			tmpStr += sprintf(tmpStr, updateTrig, cprc, userID, stockID, 's');

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);
			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);

			//Set Sell Trigger Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);

			
		}
/*********************************************************************************************/
//Cancel Set Sell
/*********************************************************************************************/
		else if(!strcmp("CANCEL_SET_SELL", userCmd) && commLen == 1)
		{
			if(!validStockID(stockID = comm[0]))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,stockID,spc,spc);
			lenq(lq, logStr, logStrLen + 1);

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);

			if(!(cTrigger = getTrigger(cUser, cStock, 's') ))
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -22);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}

			removeTrigger(cTrigger);

			tmpStr += sprintf(tmpStr, deleteTrig, userID, stockID, 's');

			dbResp = dbenq(dbWq, dbStr, transID, userCmd, returned, cUser->dbID);
			sem_wait(dbResp->destReturned);
			sem_post(dbResp->srcReturned);

			//Cancel Set Sell Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//DUMPLOG
/*********************************************************************************************/
		else if(!strcmp("DUMPLOG", userCmd) && commLen == 1)
		{
			if( strlen(file = comm[0]) < 2)
			{
				responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s,%d", transID, userCmd, "FAIL", -17);
				quickSend(webSock, responseStr, responseStrLen + 1);
				continue;
			}
			
			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,spc,file,spc);
			lenq(lq, logStr, logStrLen + 1);

			logStrLen = sprintf(logStr, "%s,%s,%s", userCmd, userID, file);
			lenq(lq, logStr, logStrLen + 1);

			//DUMPLOG Successful.
			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s", transID, userCmd, "SUCC");
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
/*********************************************************************************************/
//Display Summary
/*********************************************************************************************/
		else if(!strcmp("DISPLAY_SUMMARY", userCmd) && commLen == 0)
		{
			logStrLen = sprintf(logStr, userCmdLog, getmTS(), serverName,transID,userCmd,userID,spc,spc,spc);
			lenq(lq, logStr, logStrLen + 1);

			user_stock **userStocks;
			trigger **triggers;

			numTriggers = getAllTriggers(NULL,cUser->tTree,0);
			triggers = malloc(numTriggers*sizeof(trigger *));
			numTriggers = getAllTriggers(triggers,cUser->tTree,0);

			numUserStocks = getAllUserStocks(NULL,cUser->usTree,0);
			userStocks = malloc(numUserStocks*sizeof(user_stock *));
			numUserStocks = getAllUserStocks(userStocks,cUser->usTree,0);

			strPos = triggerInfo;
			triggerInfo[0] = '\0';
			for(i=0;i<numTriggers;i++)
			{
				cTrigger = triggers[i];
				strPos += sprintf(strPos, "%s,%c,%ld,%ld\n",cTrigger->stockObj->stockID, cTrigger->type, cTrigger->amount, cTrigger->trig);
			}

			strPos = userStockInfo;
			userStockInfo[0] = '\0';
			for(i=0;i<numUserStocks;i++)
			{
				cUserStock = userStocks[i];
				strPos += sprintf(strPos, "%s,%ld\n",cUserStock->stockObj->stockID, cUserStock->num);
			}

			free(userStocks);
			free(triggers);

//Stock_ID,Adjustment,Timestamp,Command

			responseStrLen = sprintf(responseStr, "WSR|%s|%s|%s|%ld|%s|%s", transID, userCmd, "SUCC", cUser->balance, userStockInfo, triggerInfo);//, transInfo);
			quickSend(webSock, responseStr, responseStrLen + 1);
			//puts(responseStr);
		}
		else
		{
			puts("Bad command");
			//Command Failed.
			responseStrLen = sprintf(responseStr, "WSR|%s|BADCMD|%s,%d", transID, "FAIL", -20);
			quickSend(webSock, responseStr, responseStrLen + 1);
		}
		//cUser->transCount++;
//		printf("%ld	%s\n", threadID, transID);
	}

	return 0;
}

void *trigger_handler(void *trigArgs)
{
	db_queue *dbWq = ((struct trig_args *)trigArgs)->dbWrites;
	stock_list *sList = ((struct trig_args *)trigArgs)->stocksList;
	log_queue *lq = ((struct trig_args *)trigArgs)->logs;
	quote_queue *qq = ((struct trig_args *)trigArgs)->quotes;

	sem_t *returned = malloc(sizeof(sem_t));
	if (sem_init(returned, 0, 0))
	{
		perror("semaphore init failed");
		pthread_exit(NULL);
	}

	sem_t timedWait;
	if (sem_init(&timedWait, 0, 0))
	{
		perror("semaphore init failed");
		pthread_exit(NULL);
	}/*
	struct timespec tw;
	tw.tv_sec = 0;
	tw.tv_nsec = 1000000;
*/
	char *SSA = "SET_SELL_AMOUNT";
	char *SBA = "SET_BUY_AMOUNT";
	char *userCmd;

	char *tmpStr;
	char *dbStr = malloc(5000);
	char *logStr = malloc(500);
	long int logStrLen;

	char *strPrcNew = malloc(20);
	char *strPrcMod = malloc(20);
	quote_queue_element *quoteResp;
	long int quotePrice;
	char *userID;
	char *stockID;
	char buysell;
	long int amountPrc;
	long int trigPrc;
	char *transID;
	long int sTrigMod;
	long int numStock;

	user *cUser;
	stock *cStock;
	trigger *cTrigger;
	trigger *sTrigger;
	user_stock *cUserStock;

	db_queue_element *dbResp;

	while(!sList->curr) usleep(1);
	cStock = sList->curr;

	while(1)
	{
		pthread_mutex_lock(sList->busy);
		cStock = cStock->next;
		pthread_mutex_unlock(sList->busy);

		pthread_mutex_lock(&cStock->busy);
		sTrigger = cStock->curr;
		if(!sTrigger)
		{
			pthread_mutex_unlock(&cStock->busy);
			continue;
		}

		stockID = cStock->stockID;
		userID = sTrigger->userObj->userID;
		transID = sTrigger->transID;
		if(sTrigger->type == 'b') userCmd = SBA;
		else userCmd = SSA;

		if((getmTS() - cStock->ts) > quoteTo)
		{
			quoteResp = qenq(qq, cStock, userID, transID, userCmd, returned);
			sem_wait(quoteResp->returned);
		}
		quotePrice = cStock->price;

		if(quotePrice < 1)
		{
			pthread_mutex_unlock(&cStock->busy);
			continue;			
		}

		do{
			sTrigMod = 0;
			cTrigger = cStock->curr;
			cStock->curr = cStock->curr->next;

			cUser = cTrigger->userObj;
			if(pthread_mutex_trylock(&cUser->busy))
			{
				pthread_mutex_unlock(&cStock->busy);
				continue;
			}
			pthread_mutex_unlock(&cStock->busy);

			buysell = cTrigger->type;
			trigPrc = cTrigger->trig;

			if(buysell == 'b' && quotePrice < trigPrc)
			{
				amountPrc = cTrigger->amount;
				transID = cTrigger->transID;
				userID = cUser->userID;
				cUserStock = getUserStock(cUser, cStock);

				numStock = amountPrc/quotePrice;
				tmpStr = dbStr;

				if(!numStock)
				{
					pthread_mutex_lock(&cStock->busy);
					pthread_mutex_unlock(&cUser->busy);
					continue;
				}
				long int tbuy = quotePrice*numStock;
				strPrcMod = price2str(strPrcMod, tbuy);

				logStrLen = sprintf(logStr, accTransLog, getmTS(), serverName,transID,"remove",userID,strPrcMod);
				lenq(lq, logStr, logStrLen + 1);

				if ( !cUserStock )
				{
					cUserStock = addUserStock(cUser, cStock, numStock);
					tmpStr += sprintf(tmpStr, insertStk, userID, stockID, cUserStock->num);
				}
				else
				{
					cUserStock->num = cUserStock->num + numStock;
					tmpStr += sprintf(tmpStr, updateStk, cUserStock->num, userID, stockID);
				}

				cUser->balance = cUser->balance + amountPrc - tbuy;
				strPrcNew = price2str(strPrcNew, cUser->balance);

				tmpStr += sprintf(tmpStr, updateBal, strPrcNew, userID);

				tmpStr += sprintf(tmpStr, insertTran, userID, transID, stockID, strPrcMod, getmTS(), 'g');

				pthread_mutex_lock(&cStock->busy);
				if(sTrigger == cTrigger)
				{
					sTrigger = sTrigger->next;
					sTrigMod = 1;
				}
				pthread_mutex_unlock(&cStock->busy);

				removeTrigger(cTrigger);

				tmpStr += sprintf(tmpStr, deleteTrig, userID, stockID, buysell);

				dbResp = dbenq(dbWq, dbStr, transID, "SET_BUY_AMOUNT", returned, cUser->dbID);
				sem_wait(dbResp->destReturned);
				sem_post(dbResp->srcReturned);
			}
			else if(buysell == 's' && quotePrice > trigPrc && trigPrc)
			{
				amountPrc = cTrigger->amount;
				transID = cTrigger->transID;
				userID = cUser->userID;
				cUserStock = getUserStock(cUser, cStock);

				numStock = amountPrc/quotePrice;
				tmpStr = dbStr;

				if(!cUserStock || !cUserStock->num)
				{					
					pthread_mutex_lock(&cStock->busy);
					pthread_mutex_unlock(&cUser->busy);
					continue;
				}				
				numStock++;

				if(cUserStock->num < numStock) numStock = cUserStock->num;

				long int tsell = quotePrice*numStock;
				strPrcMod = price2str(strPrcMod, tsell);

				cUserStock->num = cUserStock->num - numStock;

				tmpStr += sprintf(tmpStr, updateStk, cUserStock->num, userID, stockID);

				cUser->balance = cUser->balance + tsell;
				strPrcNew = price2str(strPrcNew, cUser->balance);

				tmpStr += sprintf(tmpStr, updateBal, strPrcNew, userID);

				logStrLen = sprintf(logStr, accTransLog, getmTS(), serverName,transID,"add",userID,strPrcMod);
				lenq(lq, logStr, logStrLen + 1);

				tmpStr += sprintf(tmpStr, insertTran, userID, transID, stockID, strPrcMod, getmTS(), 'h');

				pthread_mutex_lock(&cStock->busy);
				if(sTrigger == cTrigger)
				{
					sTrigger = sTrigger->next;
					sTrigMod = 1;
				}
				pthread_mutex_unlock(&cStock->busy);

				removeTrigger(cTrigger);

				tmpStr += sprintf(tmpStr, deleteTrig, userID, stockID, buysell);

				dbResp = dbenq(dbWq, dbStr, transID, "SET_SELL_AMOUNT", returned, cUser->dbID);
				sem_wait(dbResp->destReturned);
				sem_post(dbResp->srcReturned);
			}
			pthread_mutex_lock(&cStock->busy);
			pthread_mutex_unlock(&cUser->busy);

		} while(cStock->curr && ( sTrigger != cStock->curr || sTrigMod ));
		pthread_mutex_unlock(&cStock->busy);
		//sem_timedwait(&timedWait, &tw);
		sleep(10);
	}

	return 0;
}

/*********************************************************************************************/
//Extra functions
/*********************************************************************************************/

long int getAllTriggers(trigger **triggers, trigger_tree *tTree, long int count)
{
	if(tTree->triggerObj)
	{
		if(triggers) triggers[count] = tTree->triggerObj;
		count++;
	}
	
	trigger_tree **tChild = tTree->child;

	if(tChild)
	{
		long int i;
		for(i=0;i<2*STOCK_CHARS;i++)
		{
			if(tChild[i])
			{
				count = getAllTriggers(triggers, tChild[i], count);
			}
		}
	}
	return count;
}

long int getAllUserStocks(user_stock **userStocks, user_stock_tree *usTree, long int count)
{
	if(usTree->userStockObj)
	{
		if(userStocks) userStocks[count] = usTree->userStockObj;
		count++;
	}
	
	user_stock_tree **usChild = usTree->child;

	if(usChild)
	{
		long int i;
		for(i=0;i<STOCK_CHARS;i++)
		{
			if(usChild[i])
			{
				count = getAllUserStocks(userStocks, usChild[i], count);
			}
		}
	}
	return count;
}

void removeUser(user *userObj, user_list *ul)
{
	char *userID = userObj->userID;
	long int userLen = strlen(userID);

	user_stock_tree *usRoot = userObj->usTree;
	trigger_tree *tRoot = userObj->tTree;

	user_stock **userStocks;
	trigger **triggers;

	long int i,j,addr, numTriggers, numUserStocks;

	numTriggers = getAllTriggers(NULL,tRoot,0);
	triggers = malloc(numTriggers*sizeof(trigger *));
	numTriggers = getAllTriggers(triggers,tRoot,0);
	while(numTriggers>0)removeTrigger(triggers[numTriggers--]);

	numUserStocks = getAllUserStocks(NULL,usRoot,0);
	userStocks = malloc(numUserStocks*sizeof(user_stock *));
	numUserStocks = getAllUserStocks(userStocks,usRoot,0);
	while(numUserStocks>0)removeUserStock(userStocks[numUserStocks--]);

	pthread_mutex_lock(ul->busy);
	if(userObj->next == userObj)
	{
		ul->curr = NULL;
	}
	else
	{
		if(ul->curr == userObj) ul->curr = userObj->next;
		userObj->next->prev = userObj->prev;
		userObj->prev->next = userObj->next;
	}
	ul->num--;
	pthread_mutex_unlock(ul->busy);
	sem_wait(ul->avail);
	pthread_mutex_unlock(&userObj->busy);
	pthread_mutex_destroy(&userObj->busy);

	free(userObj->usTree);
	free(userObj->tTree);

	comm_queue_element *command;
	while(userObj->commList->length)
	{
		command = cdeq(userObj->commList);
		free(command->comm);
		free(command->transID);
		free(command->userCmd);
	}
	free(userObj->commList->start);
	free(userObj->commList);

	user_tree *temp = userObj->parent;
	free(temp->userObj);
	temp->userObj = NULL;

	user_tree **rTemp;

	for(i=userLen;i>0;)
	{
		if(temp->userObj || temp->child) break;
		temp = temp->parent;
		
		addr = (long int)userID[--i] - 65;
		free(temp->child[addr]);
		temp->child[addr] = NULL;
		rTemp = temp->child;

		for(j=0;j<USER_CHARS;j++)
		{
			if(rTemp[j]) break;
		}
		if(j != USER_CHARS) break;

		free(rTemp);
		temp->child = NULL;
	}
	free(userID);
	
	ul->num--;
	return;
}

void removeTrigger(trigger *trigObj)
{
	char *stockID = trigObj->stockObj->stockID;
	long int stockLen = strlen(stockID)-1;
	long int addrType;
	if(trigObj->type == 'b') addrType = 65;
	else if(trigObj->type == 's') addrType = 65 - STOCK_CHARS;
	else return;

	pthread_mutex_lock(&trigObj->stockObj->busy);
	if(trigObj->next == trigObj)
	{
		trigObj->stockObj->curr = NULL;
	}
	else
	{
		if(trigObj->stockObj->curr == trigObj) trigObj->stockObj->curr = trigObj->next;
		trigObj->next->prev = trigObj->prev;
		trigObj->prev->next = trigObj->next;
	}
	pthread_mutex_unlock(&trigObj->stockObj->busy);

	free(trigObj->transID);

	trigger_tree *temp = trigObj->parent;
	free(temp->triggerObj);
	temp->triggerObj = NULL;

	long int i,j,addr;
	trigger_tree **rTemp;	
		
	for(i=stockLen;i>0;)
	{
		if(temp->triggerObj || temp->child) break;
		temp = temp->parent;

		addr = (long int)stockID[--i] - addrType;
		free(temp->child[addr]);
		temp->child[addr] = NULL;
		rTemp = temp->child;

		for(j=0;j<2*STOCK_CHARS;j++)
		{
			if(rTemp[j]) break;
		}
		if(j != 2*STOCK_CHARS) break;

		free(rTemp);
		temp->child = NULL;
	}
	return;
}

void removeUserStock(user_stock *userStockObj)
{
	char *stockID = userStockObj->stockObj->stockID;
	long int stockLen = strlen(stockID);

	user_stock_tree *temp = userStockObj->parent;
	free(temp->userStockObj);
	temp->userStockObj = NULL;
	
	long int i,j,addr;
	user_stock_tree **rTemp;

	for(i=stockLen;i>0;)
	{
		if(temp->userStockObj || temp->child) break;
		temp = temp->parent;
		
		addr = (long int)stockID[--i] - 65;
		free(temp->child[addr]);
		temp->child[addr] = NULL;
		rTemp = temp->child;

		for(j=0;j<STOCK_CHARS;j++)
		{
			if(rTemp[j]) break;
		}
		if(j != STOCK_CHARS) break;

		free(rTemp);
		temp->child = NULL;
	}
	return;
}

user_stock *getUserStock(user *userObj, stock *stockObj)
{
	user_stock_tree *temp = userObj->usTree;
	char *stockID = stockObj->stockID;
	long int stockLen = strlen(stockID);

	long int i, addr, c;
	for(i=0;i<stockLen;i++)
	{
		if(!temp->child) return NULL;

		c = (long int)stockID[i];
		if( c > 64 && c < 91 )
		{
			addr = c - 65;
		}
		else
		{
			return NULL;
		}

		if(!temp->child[addr]) return NULL;

		temp = temp->child[addr];
	}
	return temp->userStockObj;
}

user_stock *addUserStock(user *userObj, stock *stockObj, long int numStock)
{
	user_stock_tree *temp = userObj->usTree;
	char *stockID = stockObj->stockID;
	long int stockLen = strlen(stockID);

	long int i, addr, c;
	for(i=0;i<stockLen;i++)
	{
		if(!temp->child) temp->child = calloc(STOCK_CHARS, sizeof(user_stock_tree *));

		c = (long int)stockID[i];
		if( c > 64 && c < 91 )
		{
			addr = c - 65;
		}
		else
		{
			return NULL;
		}

		if(!temp->child[addr])
		{
			user_stock_tree *nTemp = calloc(1, sizeof(user_stock_tree));
			nTemp->parent = temp;
			temp->child[addr] = nTemp;
		}
		temp = temp->child[addr];
	}

	if(temp->userStockObj)
	{
		return temp->userStockObj;
	}

	user_stock *nUserStock = calloc(1, sizeof(user_stock));
	nUserStock->stockObj = stockObj;
	nUserStock->num = numStock;
	nUserStock->parent = temp;

	temp->userStockObj = nUserStock;

	return temp->userStockObj;
}

trigger *getTrigger(user *userObj, stock *stockObj, char type)
{
	trigger_tree *temp = userObj->tTree;
	char *stockID = stockObj->stockID;
	long int stockLen = strlen(stockID);

	long int i, addr, c;
	for(i=0;i<stockLen;i++)
	{
		if(!temp->child) return NULL;

		c = (long int)stockID[i];
		if( c > 64 && c < 91 )
		{
			if(type == 'b') addr = c - 65;
			else if(type == 's') addr = STOCK_CHARS + c - 65;
			else return NULL;
		}
		else
		{
			return NULL;
		}

		if(!temp->child[addr]) return NULL;

		temp = temp->child[addr];
	}
	return temp->triggerObj;
}

trigger *addTrigger(user *userObj, char *transID, stock *stockObj, char type, long int amount, long int trigPrc)
{
	trigger_tree *temp = userObj->tTree;
	char *stockID = stockObj->stockID;
	long int stockLen = strlen(stockID);

	long int i, addr, c;
	for(i=0;i<stockLen;i++)
	{
		if(!temp->child) temp->child = calloc(2*STOCK_CHARS, sizeof(trigger_tree *));

		c = (long int)stockID[i];
		if( c > 64 && c < 91 )
		{
			if(type == 'b') addr = c - 65;
			else if(type == 's') addr = STOCK_CHARS + c - 65;
			else return NULL;
		}
		else
		{
			return NULL;
		}

		if(!temp->child[addr])
		{
			trigger_tree *nTemp = calloc(1, sizeof(trigger_tree));
			nTemp->parent = temp;
			temp->child[addr] = nTemp;
		}
		temp = temp->child[addr];
	}

	if(temp->triggerObj)
	{
		return temp->triggerObj;
	}

	trigger *nTrigger = calloc(1, sizeof(trigger));

	nTrigger->amount = amount;
	nTrigger->trig = trigPrc;
	nTrigger->type = type;
	nTrigger->transID = malloc(strlen(transID) + 1);
	strcpy(nTrigger->transID, transID);
	nTrigger->userObj = userObj;
	nTrigger->stockObj = stockObj;
	nTrigger->parent = temp;

	pthread_mutex_lock(&stockObj->busy);
	if(!stockObj->curr)
	{
		nTrigger->next = nTrigger;
		nTrigger->prev = nTrigger;
		stockObj->curr = nTrigger;
	}
	else
	{
		nTrigger->next = stockObj->curr;
		nTrigger->prev = stockObj->curr->prev;
		nTrigger->next->prev = nTrigger;
		nTrigger->prev->next = nTrigger;
	}
	pthread_mutex_unlock(&stockObj->busy);

	temp->triggerObj = nTrigger;

	return nTrigger;
}

stock *addStock(stock_tree *st, pthread_mutex_t *sTreeBusy, stock_list *sl, char *stockID)
{
	stock_tree *temp = st;
	long int stockLen = strlen(stockID);
	long int i, addr, c;

	pthread_mutex_lock(sTreeBusy);

	for(i=0;i<stockLen;i++)
	{
		if(!temp->child) temp->child = calloc(STOCK_CHARS, sizeof(stock_tree *));

		c = (long int)stockID[i];
		if( c > 64 && c < 91 )
		{
			addr = c - 65;
		}
		else
		{
			pthread_mutex_unlock(sTreeBusy);
			return NULL;
		}

		if(!temp->child[addr])
		{
			stock_tree *nTemp = calloc(1, sizeof(stock_tree));
			nTemp->parent = temp;
			temp->child[addr] = nTemp;
		}
		temp = temp->child[addr];
	}

	if(temp->stockObj)
	{
		pthread_mutex_unlock(sTreeBusy);
		return temp->stockObj;
	}

	stock *nStock = calloc(1,sizeof(stock));
	if (pthread_mutex_init(&nStock->busy, NULL))
	{
		perror("mutex init failed");
		pthread_mutex_unlock(sTreeBusy);
		free(nStock);
		return NULL;
	}
	pthread_mutex_lock(&nStock->busy);

	nStock->stockID = malloc(stockLen + 1);
	strcpy(nStock->stockID, stockID);
	nStock->key = malloc(57);
	nStock->parent = temp;
	
	pthread_mutex_lock(sl->busy);
	if(!sl->curr)
	{
		nStock->next = nStock;
		nStock->prev = nStock;
		sl->curr = nStock;
	}
	else
	{
		nStock->prev = sl->curr->prev;
		nStock->next = sl->curr;
		nStock->next->prev = nStock;
		nStock->prev->next = nStock;
	}
	pthread_mutex_unlock(sl->busy);

	temp->stockObj = nStock;

	pthread_mutex_unlock(sTreeBusy);
	pthread_mutex_unlock(&nStock->busy);

	return nStock;
}

stock *getStock(stock_tree *st, pthread_mutex_t *sTreeBusy, char *stockID)
{
	stock_tree *temp = st;
	long int stockLen = strlen(stockID);
	long int i, addr, c;

	pthread_mutex_lock(sTreeBusy);

	for(i=0;i<stockLen;i++)
	{
		if(!temp->child)
		{
			pthread_mutex_unlock(sTreeBusy);
			return NULL;
		}
		c = (long int)stockID[i];
		if( c > 64 && c < 91 )
		{
			addr = c - 65;
		}
		else
		{
			pthread_mutex_unlock(sTreeBusy);
			return NULL;
		}

		if(!temp->child[addr])
		{
			pthread_mutex_unlock(sTreeBusy);
			return NULL;
		}
		temp = temp->child[addr];
	}
	pthread_mutex_unlock(sTreeBusy);
	return temp->stockObj;
}

user *addUser(user_tree *ut, pthread_mutex_t *uTreeBusy, user_list *ul, char *userID, stock_tree *sTree, pthread_mutex_t *sTreeBusy, stock_list *sList, db_queue *dbRq, char *transID, char *userCmd, sem_t *returned)
{
	user_tree *temp = ut;
	long int userLen = strlen(userID);
	long int i, addr, c;

	pthread_mutex_lock(uTreeBusy);

	for(i=0;i<userLen;i++)
	{
		if(!temp->child) temp->child = calloc(USER_CHARS, sizeof(user_tree *));

		c = (long int)userID[i];
		if( c > 47 && c < 58 )
		{
			addr = c - 48;
		}
		else if( c > 96 && c < 123 )
		{
			addr = c - 87;
		}
		else if( c > 64 && c < 91 )
		{
			addr = c - 29;
		}
		else
		{
			pthread_mutex_unlock(uTreeBusy);
			return NULL;
		}

		if(!temp->child[addr])
		{
			user_tree *nTemp = calloc(1, sizeof(user_tree));
			nTemp->parent = temp;
			temp->child[addr] = nTemp;
		}

		temp = temp->child[addr];
	}

	if(temp->userObj)
	{
		pthread_mutex_unlock(uTreeBusy);
		return temp->userObj;
	}

	user *nUser = calloc(1, sizeof(user));
	if(pthread_mutex_init(&nUser->busy, NULL))
	{
		perror("mutex init failed");
		pthread_mutex_unlock(uTreeBusy);
		free(nUser);
		return NULL;
	}
	pthread_mutex_lock(&nUser->busy);

	comm_queue *cList = calloc(1, sizeof(comm_queue));
	cList->start = calloc(1, sizeof(comm_queue_element));
	cList->end = cList->start;
	if(pthread_mutex_init(&cList->busy, NULL))
	{
		perror("mutex init failed");
		free(cList->start);
		free(cList);
		pthread_mutex_unlock(uTreeBusy);
		pthread_mutex_unlock(&nUser->busy);
		free(nUser);
		return NULL;
	}
	nUser->commList = cList;

	nUser->userID = malloc(userLen + 1);
	strcpy(nUser->userID, userID);
	
	nUser->dbID = userSort(userID);
	nUser->parent = temp;
	nUser->tTree = calloc(1, sizeof(trigger_tree));
	nUser->usTree = calloc(1, sizeof(user_stock_tree));

	pthread_mutex_lock(ul->busy);
	if(!ul->curr)
	{
		nUser->next = nUser;
		nUser->prev = nUser;
		ul->curr = nUser;
	}
	else
	{
		nUser->prev = ul->curr->prev;
		nUser->next = ul->curr;
		nUser->next->prev = nUser;
		nUser->prev->next = nUser;
	}
	ul->num++;
	pthread_mutex_unlock(ul->busy);

	temp->userObj = nUser;

	pthread_mutex_unlock(uTreeBusy);

	char *dbReq = malloc(500);
	db_queue_element *dbResp;
	PGresult *result;
	long int dbRowsNum;//, dbColsNum;
	
	char *stockID;
	char *buysell;
	long int amountPrc;
	long int trigPrc;
	long int num;
	char *tID;
	stock *cStock;

	sprintf(dbReq, selectUserTrig, userID);
	dbResp = dbenq(dbRq, dbReq, transID, userCmd, returned, nUser->dbID);

	sem_wait(dbResp->destReturned);
	if(!(result = dbResp->result))
	{
		puts("Get triggers error");
	}
	else
	{
		dbRowsNum = PQntuples(result);
		
		for(i=0;i<dbRowsNum;i++)
		{
			tID = PQgetvalue(result, i, 1);
			stockID = PQgetvalue(result, i, 2);
			buysell = PQgetvalue(result, i, 3);
			amountPrc = str2price(PQgetvalue(result, i, 4));
			trigPrc = str2price(PQgetvalue(result, i, 5));

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);
			addTrigger(nUser, tID, cStock, buysell[0], amountPrc, trigPrc);
		}
	}
	sem_post(dbResp->srcReturned);

	sprintf(dbReq, selectUserStock, userID);
	dbResp = dbenq(dbRq, dbReq, transID, userCmd, returned, nUser->dbID);
	
	sem_wait(dbResp->destReturned);
	if(!(result = dbResp->result))
	{
		puts("Get triggers error");
	}
	else
	{
		dbRowsNum = PQntuples(result);
		
		for(i=0;i<dbRowsNum;i++)
		{
			stockID = PQgetvalue(result, i, 1);
			num = strtol(PQgetvalue(result, i, 2),NULL,10);

			if(!(cStock = getStock(sTree, sTreeBusy, stockID) )) cStock = addStock(sTree, sTreeBusy, sList, stockID);
			addUserStock(nUser, cStock, num);
		}
	}
	sem_post(dbResp->srcReturned);
	
	sprintf(dbReq, selectUserBal, userID);
	dbResp = dbenq(dbRq, dbReq, transID, userCmd, returned, nUser->dbID);
	
	sem_wait(dbResp->destReturned);
	if(!(result = dbResp->result))
	{
		puts("Get triggers error");
	}
	else
	{
		dbRowsNum = PQntuples(result);
		if(dbRowsNum) nUser->balance = str2price(PQgetvalue(result, 0, 0));
	}
	sem_post(dbResp->srcReturned);

	free(dbReq);

	pthread_mutex_unlock(&nUser->busy);
	return nUser;
}

user *getUser(user_tree *ut, pthread_mutex_t *uTreeBusy, char *userID)
{
	user_tree *temp = ut;
	long int userLen = strlen(userID);
	long int i, addr, c;

	pthread_mutex_lock(uTreeBusy);

	for(i=0;i<userLen;i++)
	{
		if(!temp->child)
		{
			pthread_mutex_unlock(uTreeBusy);
			return NULL;
		}

		c = (long int)userID[i];
		if( c > 47 && c < 58 )
		{
			addr = c - 48;
		}
		else if( c > 96 && c < 123 )
		{
			addr = c - 87;
		}
		else if( c > 64 && c < 91 )
		{
			addr = c - 29;
		}
		else
		{
			pthread_mutex_unlock(uTreeBusy);
			return NULL;
		}
		if(!temp->child[addr])
		{
			pthread_mutex_unlock(uTreeBusy);
			return NULL;
		}
		temp = temp->child[addr];
	}

	pthread_mutex_unlock(uTreeBusy);
	return temp->userObj;
}

long long getmTS()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	long long ts = ( ( (long long)(tv.tv_sec) ) * 1000) + ( ( (long long)(tv.tv_usec) )/1000);
	return ts;
//can also multiply by 1049 then shift right by 20 instead of dividing by 1000
}

char *price2str(char *str, long int prc)
{
	long int dollars = prc/100;
	long int cents = prc - (dollars*100);
	if(cents < 10) sprintf(str, "%ld.0%ld", dollars, cents);
	else sprintf(str, "%ld.%ld", dollars, cents);
	return str;
}

long int str2price(char *str)
{
	if(str == NULL) return -1;
	long int dollars = 0, cents = 0, i = 0, dec = 3, prclen = strlen(str);

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

	for(;i<prclen-dec;i++)
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

//Separates a string "in" to an array "out" based on a delimeter character.
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

long int delimSepMaxDelim(char *in, char **out, char delim, char maxDelim)
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
			if(count == maxDelim) break;
		}
	}
	out[count++] = in + start;
	return count;
}

comm_queue_element *cenq(comm_queue *cq, char *transID, char *userCmd, char *comm, long int *sock, user_list *ul)
{
	pthread_mutex_lock(&cq->busy);
	if(comm)
	{
		cq->end->comm = malloc(strlen(comm) + 1);
		strcpy(cq->end->comm, comm);
	}
	cq->end->transID = malloc(strlen(transID) + 1);
	cq->end->userCmd = malloc(strlen(userCmd) + 1);
	cq->end->webServerSock = sock;
	strcpy(cq->end->transID, transID);
	strcpy(cq->end->userCmd, userCmd);
	comm_queue_element *element = cq->end;
	cq->end->next = calloc(1, sizeof(comm_queue_element));
	cq->end = cq->end->next;
	cq->length++;
	pthread_mutex_unlock(&cq->busy);
	sem_post(ul->avail);
	return element;
}

comm_queue_element *cdeq(comm_queue *cq)
{
	pthread_mutex_lock(&cq->busy);
	if(!cq->start->webServerSock)
	{
		pthread_mutex_unlock(&cq->busy);
		return NULL;
	}
	comm_queue_element *element = cq->start;
	cq->start = cq->start->next;
	cq->length--;
	pthread_mutex_unlock(&cq->busy);
	return element;
}

char *quickSend(long int *sock, char *str, long int sendLen)
{
	if(!strcmp(str, "REC")){
		if( send(*sock, str, sendLen, 0) < 1)
		{
			puts("send failed");
			return NULL;
		}
	}
	return str;
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

void lenq(log_queue *lq, char *str, long int strsize)
{
	pthread_mutex_lock(lq->busy);
	lq->end->log = malloc(strsize);
	strcpy(lq->end->log, str);
	lq->end->next = calloc(1, sizeof(log_queue_element));
	lq->end = lq->end->next;
	sem_post(lq->length);
	pthread_mutex_unlock(lq->busy);
	return;
}

char *ldeq(log_queue *lq)
{
	sem_wait(lq->length);
	pthread_mutex_lock(lq->busy);
	if(!lq->start->log)
	{
		pthread_mutex_unlock(lq->busy);
		return NULL;
	}
	char *strlog = lq->start->log;
	log_queue_element *element = lq->start;
	lq->start = lq->start->next;
	free(element);
	pthread_mutex_unlock(lq->busy);
	return strlog;
}

void *dbWrite_handler(void *dbWriteArgs)
{
	db_queue *dbWq = ((struct dbWrite_args *)dbWriteArgs)->dbWrites;
	//log_queue *lq = ((struct dbWrite_args *)dbWriteArgs)->logs;

	sem_t *returned = malloc(sizeof(sem_t));
	if (sem_init(returned, 0, 0))
	{
		perror("semaphore init failed");
		pthread_exit(NULL);
	}

	db_queue_element *dbWrite;
	char *request;
	long int dbID;
	//char *transID;
	//char *userCmd;

	ExecStatusType resStatus;
	PGresult *result;
	PGconn *db[NUM_DB];
	long int i;
	for(i=0;i<NUM_DB;i++) db[i] = PQconnectdb(dbConnInfo[i]);

	puts("DBwrite thread init");
	
	dbWrite = dbdeq(dbWq);
	request = dbWrite->request;
	dbID = dbWrite->dbID;
	//transID = dbWrite->transID;
	//userCmd = dbWrite->userCmd;
	
	while(1)
	{
		if(PQstatus(db[dbID]) != CONNECTION_OK)
		{
			puts("No connection to database");
			db[dbID] = PQconnectdb(dbConnInfo[dbID]);
			usleep(1);
			continue;
		}

		result = PQexec(db[dbID], request);
		resStatus = PQresultStatus(result);

		if(resStatus != PGRES_COMMAND_OK && resStatus != PGRES_TUPLES_OK)// && resStatus != PGRES_FATAL_ERROR)
		{
			printf("Error %d\n", resStatus);
			puts(PQresultErrorMessage(result));
			PQclear(result);
			continue;
		}		

		dbWrite->result = result;
		dbWrite->srcReturned = returned;
		sem_post(dbWrite->destReturned);
		sem_wait(dbWrite->srcReturned);

		PQclear(result);
		
		free(dbWrite);
		
		dbWrite = dbdeq(dbWq);
		request = dbWrite->request;
		dbID = dbWrite->dbID;
		//transID = dbWrite->transID;
		//userCmd = dbWrite->userCmd;
	}

	return 0;
}

db_queue_element *dbenq(db_queue *q, char *request, char *transID, char *userCmd, sem_t *returned, long int dbID)
{
	pthread_mutex_lock(q->busy);
	q->end->request = request;
	q->end->transID = transID;
	q->end->userCmd = userCmd;
	q->end->dbID = dbID;
	q->end->destReturned = returned;
	db_queue_element *element = q->end;
	q->end->next = calloc(1, sizeof(db_queue_element));
	q->end = q->end->next;
	sem_post(q->length);
	pthread_mutex_unlock(q->busy);
	return element;
}

db_queue_element *dbdeq(db_queue *q)
{
	sem_wait(q->length);
	pthread_mutex_lock(q->busy);
	if(!q->start->request)
	{
		pthread_mutex_unlock(q->busy);
		return NULL;
	}
	db_queue_element *element = q->start;
	q->start = q->start->next;
	pthread_mutex_unlock(q->busy);
	return element;
}

void *dbRead_handler(void *dbReadArgs)
{
	db_queue *dbRq = ((struct dbRead_args *)dbReadArgs)->dbReads;
	//log_queue *lq = ((struct dbRead_args *)dbReadArgs)->logs;

	sem_t *returned = malloc(sizeof(sem_t));
	if (sem_init(returned, 0, 0))
	{
		perror("semaphore init failed");
		pthread_exit(NULL);
	}

	db_queue_element *dbRead;
	char *request;
	long int dbID;
	//char *transID;
	//char *userCmd;

	ExecStatusType resStatus;
	PGresult *result;
	PGconn *db[NUM_DB];
	long int i;
	for(i=0;i<NUM_DB;i++) db[i] = PQconnectdb(dbConnInfo[i]);

	puts("DBread thread init");
	
	dbRead = dbdeq(dbRq);
	request = dbRead->request;
	dbID = dbRead->dbID;
	//transID = dbRead->transID;
	//userCmd = dbRead->userCmd;
	
	while(1)
	{
		if(PQstatus(db[dbID]) != CONNECTION_OK)
		{
			puts("No connection to database");
			db[dbID] = PQconnectdb(dbConnInfo[dbID]);
			usleep(1);
			continue;
		}

		result = PQexec(db[dbID], request);
		resStatus = PQresultStatus(result);

		if(resStatus != PGRES_COMMAND_OK && resStatus != PGRES_TUPLES_OK)// && resStatus != PGRES_FATAL_ERROR)
		{
			printf("Error %d\n", resStatus);
			puts(PQresultErrorMessage(result));
			PQclear(result);
			continue;
		}		

		dbRead->result = result;
		dbRead->srcReturned = returned;
		sem_post(dbRead->destReturned);
		sem_wait(dbRead->srcReturned);

		PQclear(result);
		
		free(dbRead);
		
		dbRead = dbdeq(dbRq);
		request = dbRead->request;
		dbID = dbRead->dbID;
		//transID = dbRead->transID;
		//userCmd = dbRead->userCmd;
	}

	return 0;
}

void *quote_handler(void *quoteArgs)
{
	quote_queue *qq = ((struct quote_args *)quoteArgs)->quotes;
	//log_queue *lq = ((struct quote_args *)quoteArgs)->logs;

	char *request = malloc(200);
	char *response = malloc(200);
	long int requestLen;
	char **quoteResponse = calloc(5, sizeof(char *));
	char *transID;
	char *userCmd;
	char *userID;
	stock *stockObj;

	quote_queue_element *quoteReq;
	long int respLen;
	struct addrinfo serverSide,*loadBalHostInfo[NUM_LB];
	long int loadBalSock[NUM_LB];
	long int connected[NUM_LB];
	long int noneConnected = 0;
	long int i, j, currLB = 0, isCached = 0;

	for(i=0;i<NUM_LB;i++)
	{
		connected[i] = 0;
		loadBalSock[i] = 0;
	}

	memset(&serverSide, 0, sizeof(serverSide) );
	serverSide.ai_family = AF_INET;
	serverSide.ai_socktype = SOCK_STREAM;
	
	puts("DBread thread init");
	
	quoteReq = qdeq(qq);
	stockObj = quoteReq->stockObj;
	userID = quoteReq->userID;
	transID = quoteReq->transID;
	userCmd = quoteReq->userCmd;
	requestLen = sprintf(request, quoteRequest, transID, userCmd, stockObj->stockID, userID);
	
	while(1)
	{
		for(i=0;i<NUM_LB;i++)
		{
			if(!connected[i])
			{
				if(getaddrinfo(loadBalHost[i],loadBalPort,&serverSide,&loadBalHostInfo[i]))
				{
					puts("Get addr error\n");
					if(i == currLB) currLB = (currLB + 1) % NUM_LB;
					noneConnected = 1;
					for(j=0;j<NUM_LB;j++)
					{
						if(connected[j]) noneConnected = 0;
					}
					usleep(1);
					continue;
				}

				loadBalSock[i] = socket(loadBalHostInfo[i]->ai_family, loadBalHostInfo[i]->ai_socktype, loadBalHostInfo[i]->ai_protocol);
				if (loadBalSock[i] == -1)
				{
					puts("Could not create socket");
					usleep(1);
					continue;
				}

				if (connect(loadBalSock[i], loadBalHostInfo[i]->ai_addr, loadBalHostInfo[i]->ai_addrlen) < 0)
				{
					perror("connect failed. Error");
					close(loadBalSock[i]);
					connected[i] = 0;
					usleep(1);
					continue;
				}
				connected[i] = 1;
			}

			if( send(loadBalSock[i], request, requestLen + 1, 0) < 1)
			{
				puts("Send failed");
				connected[i] = 0;
				close(loadBalSock[i]);
				continue;
			}

			if( recv(loadBalSock[i], response, 200, 0) < 1)
			{
				puts("recv failed");
				connected[i] = 0;
				close(loadBalSock[i]);
				continue;
			}

			respLen = delimSepNoCpy(response, quoteResponse, ',');
			if(respLen == 5)
			{
				stockObj->price = str2price(quoteResponse[0]);
				stockObj->ts = strtoll(quoteResponse[3],NULL,10);
				strcpy(stockObj->key, quoteResponse[4]);
			
				isCached = 1;
				break;
			}
		}

		if(noneConnected) continue;

		if(!isCached)
		{
			isCached = 0;
			request[1] = 'N';

			if( send(loadBalSock[currLB], request, requestLen + 1, 0) < 1)
			{
				puts("Send failed");
				connected[currLB] = 0;
				close(loadBalSock[currLB]);
				request[1] = 'C';
				continue;
			}
			request[1] = 'C';

			if( recv(loadBalSock[currLB], response, 200, 0) < 1)
			{
				puts("recv failed");
				connected[currLB] = 0;
				close(loadBalSock[currLB]);
				continue;
			}

			delimSepNoCpy(response, quoteResponse, ',');
			stockObj->price = str2price(quoteResponse[0]);
			stockObj->ts = strtoll(quoteResponse[3],NULL,10);
			strcpy(stockObj->key, quoteResponse[4]);
		}
		isCached = 0;

		sem_post(quoteReq->returned);
		free(quoteReq);
		currLB = (currLB + 1) % NUM_LB;

		quoteReq = qdeq(qq);
		stockObj = quoteReq->stockObj;
		userID = quoteReq->userID;
		transID = quoteReq->transID;
		userCmd = quoteReq->userCmd;
		requestLen = sprintf(request, quoteRequest, transID, userCmd, stockObj->stockID, userID);
	}

	return 0;
}

quote_queue_element *qenq(quote_queue *q, stock *stockObj, char *userID, char *transID, char *userCmd, sem_t *returned)
{
	pthread_mutex_lock(q->busy);
	q->end->stockObj = stockObj;
	q->end->userID = userID;
	q->end->transID = transID;
	q->end->userCmd = userCmd;
	q->end->returned = returned;
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
	if(!q->start->stockObj)
	{
		pthread_mutex_unlock(q->busy);
		return NULL;
	}
	quote_queue_element *element = q->start;
	q->start = q->start->next;
	pthread_mutex_unlock(q->busy);
	return element;
}

char *validUserID(char *userID)
{
	long int len, i, c;
	if(!userID) return NULL;
	else if((len = strlen(userID)) != 10) return NULL;
	else
	{
		for(i=0;i<len;i++)
		{
			c = (long int)userID[i];
			if( c > 47 && c < 58 );
			else if( c > 96 && c < 123 );
			else if( c > 64 && c < 91 );
			else return NULL;
		}
		return userID;
	}
}

char *validStockID(char *stockID)
{
	long int len, i, c;
	if(!stockID) return NULL;
	else if((len = strlen(stockID)) != 3) return NULL;
	else
	{
		for(i=0;i<len;i++)
		{
			c = (long int)stockID[i];
			if( c > 64 && c < 91 );
			else return NULL;
		}
		return stockID;
	}
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
	return sum % NUM_DB;
}
