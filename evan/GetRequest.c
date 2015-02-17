/*
 * File:   webClient.c
 * Author: Adam
 *
 * Created on November 3, 2011, 1:26 AM
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
 
/*
 *
 */
int main(int argc, char** argv) {
     
    char arg[500];
    char firstHalf[500];
    char secondHalf[500];
    char request[1000];
    struct hostent *server;
    struct sockaddr_in serveraddr;
    int port = 80;
     
    strcpy(arg, argv[1]);
     
    int i;
    for (i = 0; i < strlen(arg); i++)
    {
        if (arg[i] == '/')
        {
                strncpy(firstHalf, arg, i);
                firstHalf[i] = '\0';
                break;
        }    
    }
     
    for (i; i < strlen(arg); i++)
    {
        strcat(secondHalf, &arg[i]);
        break;
    }
     
    printf("\nFirst Half: %s", firstHalf);
     
    printf("\nSecond Half: %s", secondHalf);
     
    int tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
     
    if (tcpSocket < 0)
        printf("\nError opening socket");
    else
        printf("\nSuccessfully opened socket");
     
    server = gethostbyname(firstHalf);
     
    if (server == NULL)
    {
        printf("gethostbyname() failed\n");
    }
    else
    {
        printf("\n%s = ", server->h_name);
        unsigned int j = 0;
        while (server -> h_addr_list[j] != NULL)
        {
            printf("%s", inet_ntoa(*(struct in_addr*)(server -> h_addr_list[j])));
            j++;
        }
    }
     
    printf("\n");
 
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
 
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
     
    serveraddr.sin_port = htons(port);
     
    if (connect(tcpSocket, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        printf("\nError Connecting");
    else
        printf("\nSuccessfully Connected");
   
    bzero(request, 1000);
 
    sprintf(request, "Get %s HTTP/1.1\r\n Host: %s\r\n \r\n \r\n", secondHalf, firstHalf);
     
    printf("\n%s", request);
     
    if (send(tcpSocket, request, strlen(request), 0) < 0)
        printf("Error with send()");
    else
        printf("Successfully sent html fetch request");
     
    bzero(request, 1000);
     
    recv(tcpSocket, request, 999, 0);
    printf("\n%s", request);
    printf("\nhello");
     
    close(tcpSocket);
     
    return (EXIT_SUCCESS);
}
