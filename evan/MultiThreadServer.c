#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread
//#include<libpq-fe.h>

//const char *conninfo = "dbname = postgres";
 
//the thread function
void *connection_handler(void *);
int db_op(char *);
 
int main(int argc , char *argv[])
{
	int socket_desc , client_sock , c , *new_sock;
	struct sockaddr_in server , client;
	/*
	//Connect to 
	PGconn *test = PQconnectdb(conninfo);
	if(PQstatus(test) != CONNECTION_OK)
	{
		printf("Connection to database failed\n");
		PQfinish(test);
		return 1;
	}*/

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
	server.sin_port = htons( 8888 );

	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");
	
	//Listen
	listen(socket_desc , 3);     

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);

	while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
	{
		puts("Connection accepted");
		
		pthread_t sniffer_thread;
		new_sock = malloc(1);
		*new_sock = client_sock;
		 
		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
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
 
/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
	//Get the socket descriptor
	int sock = *(int*)socket_desc;
	int read_size;
	char *message , *client_message = malloc(100);

	//Send some messages to the client
	message = "Greetings! I am your connection handler\n";
	write(sock , message , strlen(message));

	message = "Now type something and i shall repeat what you type \n";
	write(sock , message , strlen(message));
	
	//Receive a message from client
	while( (read_size = recv(sock , client_message , 100 , 0)) > 0 )
	{
		client_message[read_size] = '\0';
		//Send the message back to client
		write(sock , client_message , strlen(client_message));
		//db_op(client_message);
		free(client_message);
		client_message = malloc(100);
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
	free(socket_desc);

	return 0;
}
/*
int db_op(char *operation){
	if(!strcmp("ADD")){
		
	}
	else if(!strcmp("QUOTE", op)){
		
	}
	else if(!strcmp("BUY")){
		
	}
	else if(!strcmp("COMMIT_BUY")){
		
	}
	else if(!strcmp("CANCEL_BUY")){
		
	}
	else if(!strcmp("SELL")){
		
	}
	else if(!strcmp("COMMIT_SELL")){
		
	}
	else if(!strcmp("CANCEL_SELL")){
		
	}
	else if(!strcmp("SET_BUY_AMOUNT")){
		
	}
	else if(!strcmp("CANCEL_SET_BUY")){
		
	}
	else if(!strcmp("SET_BUY_TRIGGER")){
		
	}
	else if(!strcmp("SET_SELL_AMOUNT")){
		
	}
	else if(!strcmp("SET_SELL_TRIGGER")){
		
	}
	else if(!strcmp("CANCEL_SET_SELL")){
		
	}
	else if(!strcmp("DUMPLOG")){
		
	}
	else if(!strcmp("DISPLAY_SUMMARY")){
		
	}
		
	return 0;
}*/
