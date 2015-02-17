#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<netdb.h>
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
#include<time.h>

//gcc -Wall transServer.c -o transServer -lpthread

typedef struct queue_element
{
	char *comm;
	long int *webServerSock;
	struct queue_element *next;
} queue_element;

typedef struct queue
{
	struct queue_element *start;
	struct queue_element *end;
	long int length;
	long int busy;
} queue;

struct conn_args
{
	struct queue *commands;
	long int *clientSock;
};

struct comm_args
{
	struct queue *commands;
	struct user_list *users;
	struct trigger_list *triggers;
	long int *loadBalSock;
	long int *dbSock;
	long int *logSock;
};

struct trig_args
{
	struct trigger_list *triggers;
	struct user_list *users;
	long int *loadBalSock;
	long int *dbSock;
	long int *logSock;
};

typedef struct trigger
{
	char *user;
	char *stock;
	long int type;
	long int trig;
	long int amount;
	struct trigger *uNext;
	struct trigger *uPrev;
	struct trigger *sNext;
	struct trigger *sPrev;
} trigger;

typedef struct trigger_list
{
	struct trigger *curr;
} trigger_list;

typedef struct active_user
{
	char *user;
	//struct trigger *triggers;
	long long prompt_time;
	char *stock;
	long int reserve;
	long int no;
	long int buy_sell;
	struct active_user *next;
	struct active_user *prev;
} active_user;

typedef struct user_list
{
	struct active_user *curr;
} user_list;

//the thread function
void *connection_handler(void *);
void *trigger_handler(void *);
void *command_handler(void *);
long int str2price(char *prc);
long int commaSep(char *in, char **out);
void cenq(queue *q, char *str, long int strsize, long int *sock);
queue_element *cdeq(queue *q);
active_user *uget(user_list *ul, char *user);
void sendlog(int sock, char *log);
char *quickSend(int sock, char *str);

const char *loghost = "localhost", *logport = "44429";

const char* quoteRequest = "%s,%s";
const char* exec_succ = "Executed [%ld] successfully. %s";
const char* exec_fail = "Execution of [%ld] failed.";

const char* selectBal = "rSELECT balance FROM accounts WHERE user_id = '%s'";
const char* selectTrans = "rSELECT TOP 10 * FROM transactions WHERE user_id = '%s'";
const char* updateBal = "wUPDATE accounts SET balance = balance %s '%s' WHERE user_id = '%s'";
const char* insertAcct = "wINSERT INTO accounts (User_ID,Balance) VALUES ('%s', %s)";
const char* insertTran = "wINSERT INTO transactions (User_ID,Stock_ID,Adjustment,Timestamp,Command) VALUES ('%s', '%s', %s, %lld, '%c')";
const char* insertStk = "wINSERT INTO stocks (User_ID,Stock_ID,Amount) VALUES ('%s', '%s', %ld)";
const char* updateStk = "wUPDATE stocks SET amount = amount %s %ld WHERE user_id = '%s' AND stock_id = '%s'";
//const char* modifyStk = "wINSERT INTO stocks (User_ID, STOCK_ID, amount) VALUES (%s, %s, %ld) ON DUPLICATE KEY UPDATE amount = amount + %ld;";
const char* selectStk = "rSELECT amount FROM stocks WHERE user_id = '%s' AND stock_id = '%s'";

const char* selectallStk = "sSELECT * FROM stocks WHERE user_id = '%s'";
const char* selectallTrig = "sSELECT * FROM triggers";

const char* updateTrig = "wUPDATE triggers SET trigger_value = %s WHERE user_id = '%s' AND stock_id = '%s' AND Trigger_Type = '%c'";
const char* selectTrig = "rSELECT trigger_value FROM triggers WHERE User_ID = '%s' AND Stock_ID = '%s' AND Trigger_Type = '%c'";
const char* selectResv = "rSELECT reserved_funds FROM triggers WHERE User_ID = '%s' AND Stock_ID = '%s' AND Trigger_Type = '%c'";
const char* insertTrig = "wINSERT INTO triggers (User_ID,Stock_ID,Trigger_Type,Reserved_Funds,Trigger_Value) SELECT '%s', '%s', '%c', %s, %s WHERE NOT EXISTS (SELECT Stock_ID FROM triggers WHERE User_ID = '%s' AND Stock_ID = '%s' AND Trigger_Type = '%c')";
const char* deleteTrig = "wDELETE FROM triggers WHERE User_ID = '%s' AND Stock_ID = '%s' AND Trigger_Type = '%c'";
 
int main( int argc , char *argv[])
{	
	queue *commandList = malloc(sizeof(queue));
	commandList->start = malloc(sizeof(queue_element));
	commandList->end = commandList->start;
	commandList->length = 0;
	commandList->busy = 0;

	user_list *uList = malloc(sizeof(user_list));

	trigger_list *tList = malloc(sizeof(trigger_list));

/*******************************************************************/
//Client setup
/*******************************************************************/
	const char *loadBalHost = "localhost", *LBport = "44420";
	const char *dbHost = "localhost", *DBport = "44421";
	struct addrinfo serverSide,*loadBalInfo,*dbInfo,*logInfo;
	long int *loadBalSock = malloc(sizeof(long int));
	long int *dbSock = malloc(sizeof(long int));
	long int *logSock = malloc(sizeof(long int));

	memset(&serverSide, 0, sizeof serverSide);
	serverSide.ai_family = AF_INET;
	serverSide.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(loadBalHost,LBport,&serverSide,&loadBalInfo) != 0){
		puts("Get addr error\n");
	}

	if(getaddrinfo(dbHost,DBport,&serverSide,&dbInfo) != 0){
		puts("Get addr error\n");
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

	*loadBalSock = socket(loadBalInfo->ai_family, loadBalInfo->ai_socktype, loadBalInfo->ai_protocol);
	if (*loadBalSock == -1)
	{
		puts("Could not create socket");
		return 1;
	}

	if (connect(*loadBalSock, loadBalInfo->ai_addr, loadBalInfo->ai_addrlen) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}
	*dbSock = socket(dbInfo->ai_family, dbInfo->ai_socktype, dbInfo->ai_protocol);
	if (*dbSock == -1)
	{
		puts("Could not create socket");
		return 1;
	}

	if (connect(*dbSock, dbInfo->ai_addr, dbInfo->ai_addrlen) < 0)
	{
		perror("connect failed. Error");
		return 1;
	}

	pthread_t buffer_thread;
	struct comm_args comms = {commandList, uList, tList, loadBalSock, dbSock, logSock};//{loadBalInfo, dbInfo, new_sock};
		 
	if( pthread_create( &buffer_thread , NULL ,  command_handler , (void*) &comms) < 0)
	{
		perror("could not create thread");
		return 1;
	}

	pthread_t trigger_thread;
	struct trig_args trigs = {tList, uList, loadBalSock, dbSock, logSock};
		 
	if( pthread_create( &trigger_thread , NULL ,  trigger_handler , (void*) &trigs) < 0)
	{
		perror("could not create thread");
		return 1;
	}

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
	server.sin_port = htons( 44422 );

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

		struct conn_args conns = {commandList, new_sock};//{loadBalInfo, dbInfo, new_sock};
		 
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) &conns) < 0)
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

	return 0;
}
 
/*********************************************************************************************/
//This will handle connection for each client
/*********************************************************************************************/
void *connection_handler(void *connArgs)
{
	//Get the socket descriptor
	long int *sock = ((struct conn_args *)connArgs)->clientSock;
	queue *q = ((struct conn_args *)connArgs)->commands;
	const char *received = "Received";
	long int read_size;
	char *client_message = malloc(100);
	puts("Connection Initialized");
	//Receive a message from client
	while( (read_size = recv(*sock, client_message, 100, 0)) > 0 )
	{
		if(read_size < 5) continue;
		if(client_message[read_size - 1] == '\0') read_size--;		
		if(client_message[read_size - 1] == '\n') read_size--;
		if(client_message[read_size - 1] == ' ') read_size--;
		client_message[read_size] = '\0';
		//puts("Command:");
		//puts(client_message);
		while(q->busy) usleep(1);
		q->busy = 1;
		cenq(q, client_message, read_size + 1, sock);
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

/*
		while( (read_size = recv(*sock, client_message, 200, MSG_DONTWAIT) ) == -1) usleep(1);
		if(read_size == 0) break;
*/

void *trigger_handler(void *trigArgs)
{
	long int *logSock = ((struct trig_args *)trigArgs)->logSock;
	long int *loadBalSock = ((struct trig_args *)trigArgs)->loadBalSock;
	long int *dbSock = ((struct trig_args *)trigArgs)->dbSock;
	//trigger_list *tList = ((struct trig_args *)trigArgs)->triggers;
	//user_list *uList = ((struct trig_args *)trigArgs)->users;

	char *tr = malloc(100);
	char **trig = malloc(sizeof(void *));
	char *userid;
	char *sym;
	char buy_sell;
	long int amount;
	long int trigger;

	while(1)
	{
		sprintf(tr, "%s", selectallTrig);
		tr = quickSend(*dbSock, tr);

		//tr = malloc(snprintf(NULL, 0, "Received") + 1);

		while( strcmp("DNE", tr) )
		{
			free(trig);
			trig = malloc(sizeof(void *)*5);
			commaSep(tr, trig);

			userid = trig[0];
			sym = trig[1];
			buy_sell = (trig[2])[0];
			amount = str2price(trig[3]);
			trigger = str2price(trig[4]);

			//active_user *au = uget(uList, userid);

			char *s = malloc(snprintf(NULL, 0, quoteRequest, sym, userid) + 1);
			sprintf(s, quoteRequest, sym, userid);
			s = quickSend(*loadBalSock, s);

			char **qr = malloc(sizeof(void *)*5);
			commaSep(s, qr);
			free(s);
			long int qp = str2price(qr[0]);
			free(qr);
			long int nsym = amount/qp;

			if(buy_sell == 'b' && qp < trigger)
			{
				if(!nsym)
				{
					sprintf(tr, "Received");
					tr = quickSend(*dbSock, tr);
					continue;
				}
				long int tbuy = qp*nsym;
printf("%ld	%s\n",tbuy,sym);
				long int buylen;
				char *buy = malloc(buylen = (snprintf(NULL, 0, "%ld", tbuy) + 2) );
				sprintf(buy, "%ld", tbuy);
				buy[buylen-1] = buy[buylen-2];
				buy[buylen-2] = buy[buylen-3];
				buy[buylen-3] = buy[buylen-4];
				buy[buylen-4] = '.';

				s = malloc(snprintf(NULL, 0, updateBal, "-", buy, userid) + 1);
				sprintf(s, updateBal, "-", buy, userid);
				s = quickSend(*dbSock, s);
				free(s);

				s = malloc(snprintf(NULL, 0, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",(long int)1,"remove",userid,buy) + 1);
				sprintf(s, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",(long int)1,"remove",userid,buy);
				sendlog(*logSock, s);
				free(s);

				s = malloc(snprintf(NULL, 0, selectStk, userid, sym) + 1);
				sprintf(s, selectStk, userid, sym);
				s = quickSend(*dbSock, s);

				if ( !strncmp(s,"DNE",3) )
				{
					free(s);
					s = malloc(snprintf(NULL, 0, insertStk, userid, sym, nsym) + 1);
					sprintf(s, insertStk, userid, sym, nsym);
					s = quickSend(*dbSock, s);
				}
				else
				{
					free(s);
					s = malloc(snprintf(NULL, 0, updateStk, "+", nsym, userid, sym) + 1);
					sprintf(s, updateStk, "+", nsym, userid, sym);
					s = quickSend(*dbSock, s);
				}
				free(s);

				long int remainder = amount - tbuy;
				long int remlen;
				char *rem = malloc(remlen = (snprintf(NULL, 0, "%ld", remainder) + 2) );
				sprintf(rem, "%ld", remainder);
				rem[remlen-1] = rem[remlen-2];
				rem[remlen-2] = rem[remlen-3];
				rem[remlen-3] = rem[remlen-4];
				rem[remlen-4] = '.';

				s = malloc(snprintf(NULL, 0, updateBal, "+", rem, userid) + 1);
				sprintf(s, updateBal, "+", rem, userid);
				s = quickSend(*dbSock, s);
				free(s);

				char *tt = malloc(snprintf(NULL, 0, insertTran, userid, sym, buy, time(NULL), 2) + 1);
				sprintf(tt, insertTran, userid, sym, buy, time(NULL), 'b');
				tt = quickSend(*dbSock, tt);
				free(tt);
				free(buy);

				s = malloc(snprintf(NULL, 0, deleteTrig, userid, sym, buy_sell) + 1);
				sprintf(s, deleteTrig, userid, sym, buy_sell);
				s = quickSend(*dbSock, s);
				free(s);
			}
			else if(buy_sell == 's' && qp > trigger)
			{
				char *bal = malloc(snprintf(NULL, 0, selectStk, userid, sym) + 1);
				sprintf(bal, selectStk, userid, sym);
				bal = quickSend(*dbSock, bal);
				long int tbal = str2price(bal);
				free(bal);

				nsym++;
				if(tbal < nsym) nsym = tbal;

				if(!nsym)
				{
					sprintf(tr, "Received");
					tr = quickSend(*dbSock, tr);
					continue;
				}
				long int tsell = qp*nsym;
printf("%ld	%s\n",tsell,sym);
				long int selllen;
				char *sell = malloc(selllen = (snprintf(NULL, 0, "%ld", tsell) + 2) );
				sprintf(sell, "%ld", tsell);
				sell[selllen-1] = sell[selllen-2];
				sell[selllen-2] = sell[selllen-3];
				sell[selllen-3] = sell[selllen-4];
				sell[selllen-4] = '.';

				s = malloc(snprintf(NULL, 0, updateBal, "+", sell, userid) + 1);
				sprintf(s, updateBal, "+", sell, userid);
				s = quickSend(*dbSock, s);
				free(s);

				s = malloc(snprintf(NULL, 0, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",(long int)1,"add",userid,sell) + 1);
				sprintf(s, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",(long int)1,"add",userid,sell);
				sendlog(*logSock, s);
				free(s);

				s = malloc(snprintf(NULL, 0, updateStk, "-", nsym, userid, sym) + 1);
				sprintf(s, updateStk, "-", nsym, userid, sym);
				s = quickSend(*dbSock, s);
				free(s);

				char *tt = malloc(snprintf(NULL, 0, insertTran, userid, sym, sell, time(NULL), 's') + 1);
				sprintf(tt, insertTran, userid, sym, sell, time(NULL), 's');
				tt = quickSend(*dbSock, tt);
				free(tt);			
				free(sell);

				s = malloc(snprintf(NULL, 0, deleteTrig, userid, sym, buy_sell) + 1);
				sprintf(s, deleteTrig, userid, sym, buy_sell);
				s = quickSend(*dbSock, s);
				free(s);
			}
			sprintf(tr, "Received");
			tr = quickSend(*dbSock, tr);
		}
		sleep(10);
	}

	return 0;
}

/*********************************************************************************************/
//This will parse through the command queue
/*********************************************************************************************/
void *command_handler(void *commArgs)
{
	long int *logSock = ((struct comm_args *)commArgs)->logSock;
	long int *loadBalSock = ((struct comm_args *)commArgs)->loadBalSock;
	long int *dbSock = ((struct comm_args *)commArgs)->dbSock;
	queue *q = ((struct comm_args *)commArgs)->commands;
	user_list *uList = ((struct comm_args *)commArgs)->users;
	//trigger_list *tList = ((struct comm_args *)commArgs)->triggers;

	queue_element *command;
	char **comm = malloc(sizeof(void *));
	long int commLen;
	//long int read_size;

	long int transid;
	char *userid;
	char *sym;
	long int prc;
	char *file;
	char *cprc;

	puts("Command Handler Init");
	while(1)
	{
		free(comm);
		while(!q->length) usleep(1);
		while(q->busy) usleep(1);
		q->busy = 1;
		command = cdeq(q);
		q->busy = 0;
puts(command->comm);
		comm = malloc(sizeof(void *)*5);
		commLen = commaSep(command->comm, comm);
		//long int csock = *command->webServerSock;
		free(command);

		if(commLen == 0)
		{
			puts("Blank command");
			continue;
		}
		else if(commLen == -1)
		{
			puts("Bad command");
			continue;
		}
		transid = strtol(comm[0],NULL,10);

		if(!strcmp("ADD", comm[1]) && commLen == 4)
		{
			char * ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," ",comm[3]) + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," ",comm[3]);
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Add
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			cprc = comm[3];

			//request users current balance
			char *bal = malloc(snprintf(NULL, 0, selectBal, userid) + 1);
			sprintf(bal, selectBal, userid);
			bal = quickSend(*dbSock, bal);

			char *s;
			if ( !strncmp(bal,"DNE",3) )
			{
				//create new user with the amount specified in the command
				s = malloc(snprintf(NULL, 0, insertAcct, userid, cprc) + 1);
				sprintf(s, insertAcct, userid, cprc);
				s = quickSend(*dbSock, s);
			}
			else
			{
				//update users current balance by the amount specified in the command
				s = malloc(snprintf(NULL, 0, updateBal, "+", cprc, userid) + 1);
				sprintf(s, updateBal, "+", cprc, userid);
				s = quickSend(*dbSock, s);
			}
			free(bal);
			free(s);

			s = malloc(snprintf(NULL, 0, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"add",userid,cprc) + 1);
			sprintf(s, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"add",userid,cprc);
			sendlog(*logSock, s);
			free(s);

			char *tt = malloc(snprintf(NULL, 0, insertTran, userid, "NUL", cprc, time(NULL), 1) + 1);
			sprintf(tt, insertTran, userid, "...", cprc, time(NULL), 'a');
			tt = quickSend(*dbSock, tt);
			free(tt);
		}
		else if(!strcmp("QUOTE", comm[1]) && commLen == 4)
		{
			char * ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," "," ") + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," "," ");
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Quote
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;

			char *s = malloc(snprintf(NULL, 0, quoteRequest, sym, userid) + 1);
			sprintf(s, quoteRequest, sym, userid);
			s = quickSend(*loadBalSock, s);
			free(s);
		}
		else if(!strcmp("BUY", comm[1]) && commLen == 5)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]) + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]);
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Buy
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;
			if( (prc = str2price(comm[4])) == -1) continue;
			cprc = comm[4];

			//request users current balance
			char *bal = malloc(snprintf(NULL, 0, selectBal, userid) + 1);
			sprintf(bal, selectBal, userid);
			bal = quickSend(*dbSock, bal);

			char *s = malloc(snprintf(NULL, 0, quoteRequest, sym, userid) + 1);
			sprintf(s, quoteRequest, sym, userid);
			s = quickSend(*loadBalSock, s);

			if ( str2price(bal) < prc )
			{
				free(s);
				free(bal);
				continue;
			}
			char **qr = malloc(sizeof(void *)*5);
			commaSep(s, qr);

			long int qp = str2price(qr[0]);
			long int nsym = prc/qp;

			if(!nsym)
			{
//Change message to "Don't have enough money, price is $$$ per stock"
				
				free(s);
				free(bal);
				free(qr);
				continue;
			}

			active_user *au = uget(uList, userid);

			if(au->buy_sell)
			{
//Change message to "You already have a pending buy/sell"
				
				free(s);
				free(bal);
				free(qr);
				continue;
			}

			long int buy = qp * nsym;

			au->prompt_time = strtoll(qr[3],NULL,10);
			au->stock = malloc(strlen(qr[1]) + 1);
			strcpy(au->stock,qr[1]);
			au->reserve = buy;
			au->no = nsym;
			au->buy_sell = 1;

//Change message to "You have enough money to buy # shares of <stock> for $$$. You have __ seconds to commit."
			
			free(qr);
			free(s);
			free(bal);
		}
		else if(!strcmp("COMMIT_BUY", comm[1]) && commLen == 3)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ") + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ");
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Commit Buy
/*********************************************************************************************/

			if( strlen(userid = comm[2]) != 10) continue;

			active_user *au = uget(uList, userid);

			if(au->buy_sell != 1)
			{
				continue;
			}

			if( ( (long long)time(NULL) - au->prompt_time) > 60000)
			{
				au->prompt_time = 0;
				free(au->stock);
				au->stock = NULL;
				au->reserve = 0;
				au->no = 0;
				au->buy_sell = 0;

				continue;
			}

			long int buylen;
			char *buy = malloc(buylen = (snprintf(NULL, 0, "%ld", au->reserve) + 2) );
			sprintf(buy, "%ld", au->reserve);
			buy[buylen-1] = buy[buylen-2];
			buy[buylen-2] = buy[buylen-3];
			buy[buylen-3] = buy[buylen-4];
			buy[buylen-4] = '.';

			char *s = malloc(snprintf(NULL, 0, updateBal, "-", buy, userid) + 1);
			sprintf(s, updateBal, "-", buy, userid);
			s = quickSend(*dbSock, s);
			free(s);

			s = malloc(snprintf(NULL, 0, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"remove",userid,buy) + 1);
			sprintf(s, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"remove",userid,buy);
			sendlog(*logSock, s);
			free(s);

			s = malloc(snprintf(NULL, 0, selectStk, userid, au->stock) + 1);
			sprintf(s, selectStk, userid, au->stock);
			s = quickSend(*dbSock, s);

			if ( !strncmp(s,"DNE",3) )
			{
				free(s);
				s = malloc(snprintf(NULL, 0, insertStk, userid, au->stock, au->no) + 1);
				sprintf(s, insertStk, userid, au->stock, au->no);
				s = quickSend(*dbSock, s);
			}
			else
			{
				free(s);
				s = malloc(snprintf(NULL, 0, updateStk, "+", au->no, userid, au->stock) + 1);
				sprintf(s, updateStk, "+", au->no, userid, au->stock);
				s = quickSend(*dbSock, s);
			}
			free(s);

			char *tt = malloc(snprintf(NULL, 0, insertTran, userid, au->stock, buy, time(NULL), 2) + 1);
			sprintf(tt, insertTran, userid, au->stock, buy, time(NULL), 'b');
			tt = quickSend(*dbSock, tt);
			free(tt);
			free(buy);

			au->prompt_time = 0;
			free(au->stock);
			au->stock = NULL;
			au->reserve = 0;
			au->no = 0;
			au->buy_sell = 0;
		}
		else if(!strcmp("CANCEL_BUY", comm[1]) && commLen == 3)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ") + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ");
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Cancel Buy
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;

			active_user *au = uget(uList, userid);

			if(au->buy_sell != 1)
			{
				
				continue;
			}

			au->prompt_time = 0;
			free(au->stock);
			au->stock = NULL;
			au->reserve = 0;
			au->no = 0;
			au->buy_sell = 0;
			
		}
		else if(!strcmp("SELL", comm[1]) && commLen == 5)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]) + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]);
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Sell
/*********************************************************************************************/

			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;
			if( (prc = str2price(comm[4])) == -1) continue;
			cprc = comm[4];

			//request users current balance
			char *bal = malloc(snprintf(NULL, 0, selectStk, userid, sym) + 1);
			sprintf(bal, selectStk, userid, sym);
			bal = quickSend(*dbSock, bal);

			char *s = malloc(snprintf(NULL, 0, quoteRequest, sym, userid) + 1);
			sprintf(s, quoteRequest, sym, userid);
			s = quickSend(*loadBalSock, s);
			
			if ( !strncmp(bal,"DNE",3) )
			{
				free(s);
				free(bal);
				continue;
			}
			char **qr = malloc(sizeof(void *)*5);
			commaSep(s, qr);

			long int qp = str2price(qr[0]);
			long int nsym = prc/qp;

			if(!nsym || strtol(bal, NULL, 10) < nsym)
			{

//Change message to "Don't have enough money, price is $$$ per stock"
				
				free(s);
				free(bal);
				free(qr);
				continue;
			}

			active_user *au = uget(uList, userid);

			if(au->buy_sell)
			{
//Change message to "You already have a pending buy/sell for <stock> "
				
				free(s);
				free(bal);
				free(qr);
				continue;
			}

			long int sell = qp * nsym;

			au->prompt_time = strtoll(qr[3],NULL,10);
			au->stock = malloc(strlen(qr[1]) + 1);
			strcpy(au->stock,qr[1]);
			au->reserve = sell;
			au->no = nsym;
			au->buy_sell = -1;

//Change message to "You have enough money to sell # shares of <stock> for $$$. You have __ seconds to commit."
			
			free(qr);
			free(s);
			free(bal);
		}
		else if(!strcmp("COMMIT_SELL", comm[1]) && commLen == 3)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ") + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ");
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Commit Sell
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			
			active_user *au = uget(uList, userid);

			if(au->buy_sell != -1)
			{
				
				continue;
			}

			if( ( (long long)time(NULL) - au->prompt_time) > 60000)
			{
				au->prompt_time = 0;
				free(au->stock);
				au->stock = NULL;
				au->reserve = 0;
				au->no = 0;
				au->buy_sell = 0;

				continue;
			}

			long int selllen;
			char *sell = malloc(selllen = (snprintf(NULL, 0, "%ld", au->reserve) + 2) );
			sprintf(sell, "%ld", au->reserve);
			sell[selllen-1] = sell[selllen-2];
			sell[selllen-2] = sell[selllen-3];
			sell[selllen-3] = sell[selllen-4];
			sell[selllen-4] = '.';

			char *s = malloc(snprintf(NULL, 0, updateBal, "+", sell, userid) + 1);
			sprintf(s, updateBal, "+", sell, userid);
			s = quickSend(*dbSock, s);
			free(s);

			s = malloc(snprintf(NULL, 0, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"add",userid,sell) + 1);
			sprintf(s, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"add",userid,sell);
			sendlog(*logSock, s);
			free(s);

			s = malloc(snprintf(NULL, 0, updateStk, "-", au->no, userid, au->stock) + 1);
			sprintf(s, updateStk, "-", au->no, userid, au->stock);
			s = quickSend(*dbSock, s);
			free(s);

			char *tt = malloc(snprintf(NULL, 0, insertTran, userid, au->stock, sell, time(NULL), 's') + 1);
			sprintf(tt, insertTran, userid, au->stock, sell, time(NULL), 's');
			tt = quickSend(*dbSock, tt);
			free(tt);			
			free(sell);

			au->prompt_time = 0;
			free(au->stock);
			au->stock = NULL;
			au->reserve = 0;
			au->no = 0;
			au->buy_sell = 0;
		}
		else if(!strcmp("CANCEL_SELL", comm[1]) && commLen == 3)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ") + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ");
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Cancel Sell
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;

			active_user *au = uget(uList, userid);

			if(au->buy_sell != -1)
			{
				
				continue;
			}

			au->prompt_time = 0;
			free(au->stock);
			au->stock = NULL;
			au->reserve = 0;
			au->no = 0;
			au->buy_sell = 0;

			
		}
		else if(!strcmp("SET_BUY_AMOUNT", comm[1]) && commLen == 5)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]) + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]);
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Set Buy Amount
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;
			if( (prc = str2price(comm[4])) == -1) continue;
			cprc = comm[4];

			active_user *au = uget(uList, userid);
			if(au->buy_sell) continue;

			char *bal = malloc(snprintf(NULL, 0, selectBal, userid) + 1);
			sprintf(bal, selectBal, userid);
			bal = quickSend(*dbSock, bal);

			if( str2price(bal) < prc )
			{
				free(bal);
				continue;
			}
			free(bal);
/*
			trigger *utrigs = au->triggers;
			if(utrigs != NULL)
			{
				while(utrigs->next != NULL)
				{

					utrigs = utrigs->next;
					if( !strcmp(utrigs->stock, sym) && utrigs->type) break;
				}
			}

			if(utrigs == NULL)
			{
				utrigs = malloc(sizeof(trigger));
			}
			else if( !strcmp(utrigs->stock, sym) && utrigs->type)
			{
				continue;
			}
			else
			{
				utrigs->next = malloc(sizeof(trigger));
				utrigs = utrigs->next;
			}
*/
			char *s = malloc(snprintf(NULL, 0, selectResv, userid, sym, 'b') + 1);
			sprintf(s, selectResv, userid, sym, 'b');
			s = quickSend(*dbSock, s);

			if( strcmp("DNE", s) )
			{
				free(s);
				continue;
			}
			free(s);

			s = malloc(snprintf(NULL, 0, updateBal, "-", cprc, userid) + 1);
			sprintf(s, updateBal, "-", cprc, userid);
			s = quickSend(*dbSock, s);
			free(s);

			s = malloc(snprintf(NULL, 0, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"remove",userid,cprc) + 1);
			sprintf(s, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"remove",userid,cprc);
			sendlog(*logSock, s);
			free(s);

			s = malloc(snprintf(NULL, 0, insertTrig, userid, sym, 'b', cprc, "0.00", userid, sym, 'b') + 1);
			sprintf(s, insertTrig, userid, sym, 'b', cprc, "0.00", userid, sym, 'b');
			s = quickSend(*dbSock, s);
			free(s);
			//Subtract from balance.
/*
			utrigs->stock = malloc(strlen(sym) + 1);
			strcpy(utrigs->stock,sym);
			utrigs->type = 1;
			utrigs->trig = -1;
			utrigs->amount = prc;*/
		}
		else if(!strcmp("CANCEL_SET_BUY", comm[1]) && commLen == 4)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," "," ") + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," "," ");
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Cancel Set Buy
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;
			/*
			active_user *au = uget(uList, userid);
			
			trigger *utrigs = au->triggers;
			if(utrigs != NULL)
			{
				while(utrigs->next != NULL)
				{
					if( !strcmp(utrigs->next->stock, sym) && utrigs->next->type) break;
					utrigs = utrigs->next;
				}
			}
			if(utrigs == NULL)
			{
				continue;
			}
			else if( !strcmp(utrigs->next->stock, sym) && utrigs->next->type)
			{*/

			char *res = malloc(snprintf(NULL, 0, selectResv, userid, sym, 1) + 1);
			sprintf(res, selectResv, userid, sym, 1);
			res = quickSend(*dbSock, res);

			if( !strcmp("DNE", res) )
			{
				free(res);
				continue;
			}

			char *s = malloc(snprintf(NULL, 0, updateBal, "+", res, userid) + 1);
			sprintf(s, updateBal, "+", res, userid);
			s = quickSend(*dbSock, s);
			free(s);

			s = malloc(snprintf(NULL, 0, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"add",userid,res) + 1);
			sprintf(s, "acctrans,%lld,%s,%ld,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,"add",userid,res);
			sendlog(*logSock, s);
			free(s);
			free(res);

			s = malloc(snprintf(NULL, 0, deleteTrig, userid, sym, 'b') + 1);
			sprintf(s, deleteTrig, userid, sym, 'b');
			s = quickSend(*dbSock, s);
			free(s);

			//trigger *temp = utrigs->next;
			//utrigs->next = temp->next;
			//free(temp);
			/*}
			else
			{
				continue;
			}*/
		}
		else if(!strcmp("SET_BUY_TRIGGER", comm[1]) && commLen == 5)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]) + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]);
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Set Buy Trigger
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;
			if( (prc = str2price(comm[4])) == -1) continue;
			cprc = comm[4];
/*
			active_user *au = uget(uList, userid);

			trigger *utrigs = au->triggers;
			if(utrigs != NULL)
			{
				while(utrigs->next != NULL)
				{
					utrigs = utrigs->next;
					if( !strcmp(utrigs->stock, sym) && utrigs->type) break;
				}
			}

			if(utrigs == NULL)
			{
				continue;
			}
			else if( !strcmp(utrigs->stock, sym) && utrigs->type)
			{

				if(utrigs->trig != -1) continue;

				utrigs->trig = prc;

			long int buylen;
			char *buy = malloc(buylen = (snprintf(NULL, 0, "%ld", utrigs->amount) + 2) );
			sprintf(buy, "%ld", utrigs->amount);
			buy[buylen-1] = buy[buylen-2];
			buy[buylen-2] = buy[buylen-3];
			buy[buylen-3] = buy[buylen-4];
			buy[buylen-4] = '.';*/

			char *s = malloc(snprintf(NULL, 0, updateTrig, cprc, userid, sym, 'b') + 1);
			sprintf(s, updateTrig, cprc, userid, sym, 'b');
			s = quickSend(*dbSock, s);
			free(s);
			/*free(buy);
			}
			else
			{
				continue;
			}*/
		}
		else if(!strcmp("SET_SELL_AMOUNT", comm[1]) && commLen == 5)
		{

			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]) + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]);
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Set Sell Amount
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;
			if( (prc = str2price(comm[4])) == -1) continue;
			cprc = comm[4];

			active_user *au = uget(uList, userid);
			if(au->buy_sell) continue;

			char *bal = malloc(snprintf(NULL, 0, selectStk, userid, sym) + 1);
			sprintf(bal, selectStk, userid, sym);
			bal = quickSend(*dbSock, bal);

			if( !strcmp("DNE", bal) )
			{
				free(bal);
				continue;
			}
			free(bal);

			/*
			trigger *utrigs = au->triggers;
			if(utrigs != NULL)
			{
				while(utrigs->next != NULL)
				{
				
					utrigs = utrigs->next;
					if( !strcmp(utrigs->stock, sym) && !(utrigs->type)) break;
				}
			}
			if(utrigs == NULL)
			{
				utrigs = malloc(sizeof(trigger));
			}
			else if( !strcmp(utrigs->stock, sym) && !(utrigs->type))
			{
				continue;
			}
			else
			{
				utrigs->next = malloc(sizeof(trigger));
				utrigs = utrigs->next;
			}*/

			char *s = malloc(snprintf(NULL, 0, selectResv, userid, sym, 's') + 1);
			sprintf(s, selectResv, userid, sym, 's');
			s = quickSend(*dbSock, s);

			if( strcmp("DNE", s) )
			{
				free(s);
				continue;
			}
			free(s);

			s = malloc(snprintf(NULL, 0, insertTrig, userid, sym, 's', cprc, "0.00", userid, sym, 's') + 1);
			sprintf(s, insertTrig, userid, sym, 's', cprc, "0.00", userid, sym, 's');
			s = quickSend(*dbSock, s);
			free(s);
/*
			utrigs->stock = malloc(strlen(sym) + 1);
			strcpy(utrigs->stock,sym);
			utrigs->type = 0;
			utrigs->trig = -1;
			utrigs->amount = prc;
*/
		}
		else if(!strcmp("SET_SELL_TRIGGER", comm[1]) && commLen == 5)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]) + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," ",comm[4]);
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Set Sell Trigger
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;
			if( (prc = str2price(comm[4])) == -1) continue;
			cprc = comm[4];
/*
			active_user *au = uget(uList, userid);
			
			trigger *utrigs = au->triggers;
			if(utrigs != NULL)
			{
				while(utrigs->next != NULL)
				{
					utrigs = utrigs->next;
					if( !strcmp(utrigs->stock, sym) && utrigs->type) break;
				}
			}
			if(utrigs == NULL)
			{
				continue;
			}
			else if( !strcmp(utrigs->stock, sym) && utrigs->type)
			{
				if(utrigs->trig != -1) continue;

				utrigs->trig = prc;

				long int selllen;
				char *sell = malloc(selllen = (snprintf(NULL, 0, "%ld", utrigs->amount) + 2) );
				sprintf(sell, "%ld", utrigs->amount);
				sell[selllen-1] = sell[selllen-2];
				sell[selllen-2] = sell[selllen-3];
				sell[selllen-3] = sell[selllen-4];
				sell[selllen-4] = '.';
*/
			char *s = malloc(snprintf(NULL, 0, updateTrig, cprc, userid, sym, 's') + 1);
			sprintf(s, updateTrig, cprc, userid, sym, 's');
			s = quickSend(*dbSock, s);
			free(s);
			/*}
			else
			{
				continue;
			}*/
		}
		else if(!strcmp("CANCEL_SET_SELL", comm[1]) && commLen == 4)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," "," ") + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2],comm[3]," "," ");
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Cancel Set Sell
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			if( strlen(sym = comm[3]) > 3) continue;
			/*
			active_user *au = uget(uList, userid);
			
			trigger *utrigs = au->triggers;
			if(utrigs != NULL)
			{
				while(utrigs->next != NULL)
				{
					if( !strcmp(utrigs->next->stock, sym) && !(utrigs->next->type)) break;
					utrigs = utrigs->next;
				}
			}
			if(utrigs == NULL)
			{
				continue;
			}
			else if( !strcmp(utrigs->next->stock, sym) && !(utrigs->next->type))
			{*/

			char *s = malloc(snprintf(NULL, 0, deleteTrig, userid, sym, 's') + 1);
			sprintf(s, deleteTrig, userid, sym, 's');
			s = quickSend(*dbSock, s);
			free(s);

				/*trigger *temp = utrigs->next;
				utrigs->next = temp->next;
				free(temp);
			}
			else
			{
				continue;
			}*/
		}
		else if(!strcmp("DUMPLOG", comm[1]) && (commLen == 4 || commLen == 3) )
		{
/*********************************************************************************************/
//DUMPLOG
/*********************************************************************************************/
			if(commLen == 4){
				char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," ",comm[3]," ") + 1);
				sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," ",comm[3]," ");
				sendlog(*logSock, ll);
				free(ll);

				if( strlen(userid = comm[2]) != 10) continue;
				if( strlen(file = comm[3]) < 2 || file[0] != '/') continue;
				
			}
			else
			{
				char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1]," "," ",comm[2]," ") + 1);
				sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1]," "," ",comm[2]," ");
				sendlog(*logSock, ll);
				free(ll);

				file = comm[2];
				ll = malloc(snprintf(NULL, 0, "DUMPLOG,%s", file) + 1);
				sprintf(ll, "DUMPLOG,%s", file);
				puts(ll);
				sendlog(*logSock, ll);
				free(ll);
				
			}
		}
		else if(!strcmp("DISPLAY_SUMMARY", comm[1]) && commLen == 3)
		{
			char *ll = malloc(snprintf(NULL, 0, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ") + 1);
			sprintf(ll, "usercmd,%lld,%s,%ld,%s,%s,%s,%s,%s", 1000*(long long)time(NULL), "TRAN1",transid,comm[1],comm[2]," "," "," ");
			sendlog(*logSock, ll);
			free(ll);
/*********************************************************************************************/
//Display Summary
/*********************************************************************************************/
			if( strlen(userid = comm[2]) != 10) continue;
			
		}
		else
		{
			
			puts("Bad command");
		}
	}

	close(*dbSock);
	free(dbSock);
	close(*loadBalSock);
	free(loadBalSock);

	return 0;
}

/*********************************************************************************************/
//Extra functions
/*********************************************************************************************/
/*
trigger *tget(trigger_list *tl, char *user, char *stock)
{
	if(tl->curr == NULL)
	{
		trigger *t = malloc(sizeof(trigger));
		tl->curr = t;
		t->user = malloc(strlen(user) + 1);
		strcpy(t->user, user);
		t->stock = malloc(strlen(stock) + 1);
		strcpy(t->stock, stock);
		t->type = 0;
		t->trig = 0;
		t->amount = 0;
		t->uNext = t;
		t->uPrev = t;
		t->sNext = t;
		t->sPrev = t;
		return t;
	}

	trigger *temp = ul->curr;

	while(strcmp(temp->stock, stock))
	{
		temp = temp->sNext;
		if(temp == tl->curr) break;
	}
	if(strcmp(temp->stock, stock))
	{

	}

	while(strcmp(temp->user, user))
	{
		temp = temp->next;
		if(temp == tl->curr) break;
	}
	if(!strcmp(temp->user, user)) return tl->curr = temp;

	trigger *t = malloc(sizeof(trigger));
	t->user = malloc(strlen(user) + 1);
	strcpy(t->user, user);
	t->stock = malloc(strlen(stock) + 1);
	strcpy(t->stock, stock);
	t->type = 0;
	t->trig = 0;
	t->amount = 0;
	t->next = temp;
	t->prev = temp->prev;
	temp->prev->next = auser;
	temp->prev = auser;

	return tl->curr = t;
}*/

active_user *uget(user_list *ul, char *user)
{
	if(ul->curr == NULL)
	{
		active_user *auser = malloc(sizeof(active_user));
		ul->curr = auser;
		auser->user = malloc(strlen(user) + 1);
		strcpy(auser->user, user);
		auser->prompt_time = 0;
		auser->reserve = 0;
		auser->no = 0;
		auser->buy_sell = 0;
		auser->next = auser;
		auser->prev = auser;
		return auser;
	}

	active_user *temp;
	temp = ul->curr;

	while(strcmp(temp->user, user))
	{
		temp = temp->next;
		if(temp == ul->curr) break;
	}
	if(!strcmp(temp->user, user)) return ul->curr = temp;

	active_user *auser = malloc(sizeof(active_user));
	auser->user = malloc(strlen(user) + 1);
	strcpy(auser->user, user);
	auser->prompt_time = 0;
	auser->reserve = 0;
	auser->no = 0;
	auser->buy_sell = 0;
	auser->next = temp;
	auser->prev = temp->prev;
	temp->prev->next = auser;
	temp->prev = auser;

	return ul->curr = auser;
}

long int str2price(char *str)
{
	if(str == NULL) return -1;
	long int dollars = 0, cents = 0, i = 0, dec = 3, prclen = (long int)strlen(str);

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

long int commaSep(char *in, char **out)
{
	if(in == NULL) return 0;
	long int i=0,count = 0,start = 0, len = (long int)strlen(in);

	for(i=0;i<len;i++)
	{
		if(in[i] == ' ')
		{
			if(in[i-1] != ']' || in[0] != '[') return -1;
			out[count] = malloc(i-1);
			strncpy(out[count], &in[1], i - 2);
			out[count][i-2] = '\0';
			start = i + 1;
			count++;
			break;
		}
	}
	if(i == len) i = -1;
	i++;
	for(;i<len;i++)
	{
		if(count == 5) return -1;
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
}

void cenq(queue *q, char *str, long int strsize, long int *sock)
{
	q->end->comm = malloc(strsize);
	q->end->webServerSock = sock;
	strcpy(q->end->comm, str);
	q->end->next = malloc(sizeof(queue_element));
	q->end = q->end->next;
	q->length = q->length + 1;
	return;
}

queue_element *cdeq(queue *q)
{
	if(q->start->comm == NULL) return NULL;
	queue_element *element = q->start;
	q->start = q->start->next;
	q->length = q->length - 1;
	return element;
}

void sendlog(int sock, char *log)
{
	//Send some data
	if( send(sock, log, strlen(log) + 1, 0) < 1)
	{
		puts("Send failed");
		return;
	}

	char *str = malloc(20);
	if( recv(sock, str, 20, 0) < 1)
	{
		puts("recv failed");
		return;
	}
	free(str);
	return;
}

char *quickSend(int sock, char *str)
{
	if( send(sock, str, strlen(str) + 1, 0) < 1)
	{
		puts("send failed");
		return NULL;
	}
	free(str);

	int read_size = 0;
	str = malloc(500);

	if( (read_size = recv(sock, str, 500, 0)) < 1)
	{
		puts("recv failed");
		return NULL;
	}
	str[read_size] = '\0';
	return str;
}
