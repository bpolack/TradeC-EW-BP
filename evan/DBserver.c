#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<netdb.h>
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
#include<libpq-fe.h>

//gcc -Wall DBserver.c -o DBserver -lpthread -I/seng/seng462/group2/postgres/include -L/seng/seng462/group2/postgres/lib -lpq
//gcc -Wall DBserver.c -o DBserver -lpthread -I/usr/include/postgresql/ -lpq

struct thread_args
{
	//struct addrinfo *logInfo;
	int *clientSock;
	int *dbBusy;
	PGconn *db;
};
int sendlog(char *log);

const char *loghost = "localhost", *logport = "44429";
const char *DNE = "DNE";
const char *Success = "Success";
//const char *conninfo = "dbname = postgres";
const char *conninfo = "dbname = seng462";
 
//the thread function
void *connection_handler(void *);
 
int main(int argc , char *argv[])
{
	int socket_desc , client_sock , c , *new_sock;
	struct sockaddr_in server , client;
	
	//Connect to
	PGconn *db = PQconnectdb(conninfo);
	if(PQstatus(db) != CONNECTION_OK)
	{
		printf("Connection to database failed\n");
		PQfinish(db);
		return 1;
	}
	puts("connected to db");
	int dbBusy = 1;
	/*PQexec(db, "DELETE FROM stocks");
	PQexec(db, "DELETE FROM triggers");
	PQexec(db, "DELETE FROM transactions");
	PQexec(db, "DELETE FROM accounts");*/
	dbBusy = 0;
	

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
	server.sin_port = htons( 44421 );

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

		struct thread_args cl = {new_sock, &dbBusy, db};
		 
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) &cl) < 0)
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
	//PQfinish(db);
	return 0;
}
 
/*
 * This will handle connection for each client
 * */
void *connection_handler(void *args)
{
	//Get the socket descriptor
	int sock = *(((struct thread_args *)args)->clientSock);
	PGconn *conn = ((struct thread_args *)args)->db;
	int *dbBusy = ((struct thread_args *)args)->dbBusy;
	
	int read_size;
	char *resp;
	char *client_message = malloc(500);
	
	//Receive a message from client
	while( (read_size = recv(sock , client_message , 500 , 0)) > 0 )
	{
		client_message[read_size] = '\0';
		//puts(client_message);
		
		while(*dbBusy == 1) usleep(1);
		*dbBusy = 1;	

		PGresult *res = PQexec(conn, (client_message + 1));
		if(client_message[0] == 'r')
		{
			if(!PQntuples(res))
			{
				send(sock, DNE, strlen(DNE) + 1, 0);
				//puts("Empty Result");
			}
			else
			{
				resp = PQgetvalue(res, 0, 0);
				//puts(resp);
				send(sock, resp, strlen(resp) + 1, 0);
			}
		}
		if(client_message[0] == 's')
		{
			int i, j, respRowLen;
			char *respRow =  malloc(100);
			int numRows = PQntuples(res);
			int numCols = PQnfields(res);
			for(i=0;i<numRows;i++)
			{
				respRow[0] = '\0';
				for(j=0;j<numCols;j++)
				{
					resp = PQgetvalue(res, i, j);
					strcat(respRow, resp);
					strcat(respRow, ",");
				}
				respRowLen = strlen(respRow);
				respRow[respRowLen - 1] = '\0';
				send(sock, respRow, respRowLen, 0);
				recv(sock, respRow, 10, 0);
			}
			send(sock, DNE, strlen(DNE) + 1, 0);
			//recv(sock, respRow, 10, 0);
			free(respRow);
		}
		else
		{	
			if(PQresultStatus(res) != PGRES_COMMAND_OK)
			{
				resp = PQerrorMessage(conn);
				puts(resp);
				send(sock, resp, strlen(resp) + 1, 0);
			}
			else
			{
				send(sock, Success, strlen(Success) + 1, 0);
			}
		}
		
		PQclear(res);
		*dbBusy = 0;
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
	free(((struct thread_args *)args)->clientSock);
	close(sock);

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
