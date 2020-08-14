#ifndef _MFOG_NET_C
#define _MFOG_NET_C

/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <err.h>
#include <stdarg.h>
#include <sys/time.h>
#include <errno.h>
#include <arpa/inet.h>

#include "net.h"

server_t *serverStart(short unsigned int port) {
    server_t *server = malloc(sizeof(server));
    server->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->serverSocket < 0)
        errx(EXIT_FAILURE, "ERROR opening socket. At "__FILE__":%d\n", __LINE__);
    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    int opt = 1;
    if (setsockopt(server->serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
        errx(EXIT_FAILURE, "ERROR on setsockopt. At "__FILE__":%d\n", __LINE__);
    //
    struct sockaddr_in address;
    socklen_t addrSize = sizeof(address);
    bzero((char *)&address, addrSize);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    //
    if (bind(server->serverSocket, (struct sockaddr *)&address, addrSize) < 0)
        errx(EXIT_FAILURE, "ERROR on binding. At "__FILE__":%d\n", __LINE__);
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(server->serverSocket, 3) < 0)
        errx(EXIT_FAILURE, "ERROR on listen. At "__FILE__":%d\n", __LINE__);
    //
    server->clients = malloc(10 * sizeof(SOCKET));
    server->clientsLen = 10;
    for (int i = 0; i < server->clientsLen; i++) {
        server->clients[i] = 0;
    }
    fprintf(stderr, "serverStart(%u)\n", port);
    return server;
}

SOCKET serverAccept(SOCKET sockfd) {
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    SOCKET newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0)
        errx(EXIT_FAILURE, "ERROR on accept. At "__FILE__":%d\n", __LINE__);
    return newsockfd;
}

void serverDisconnect(server_t *server, SOCKET connection, int connectionIndex) {
    struct sockaddr_in address;
    socklen_t addSize = sizeof(address);
    // Somebody disconnected , get his details and print
    getpeername(connection, (struct sockaddr *)&address, &addSize);
    char *addString = inet_ntoa(address.sin_addr);
    int port = ntohs(address.sin_port);
    printf("Host disconnected , ip %s , port %d \n", addString, port);

    //Close the socket and mark as 0 in list for reuse
    close(connection);
    server->clients[connectionIndex] = 0;
}

int serverSelect(server_t *server) {
    FD_ZERO(&server->readfds);
    FD_SET(server->serverSocket, &server->readfds);
    int max_sd = server->serverSocket;
    for (size_t i = 0; i < server->clientsLen; i++) {
        int sd = server->clients[i];
        if (sd > 0)
            FD_SET(sd, &server->readfds);
        if (sd > max_sd)
            max_sd = sd;
    }
    //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
    int activity = 0;
    // activity = select(max_sd + 1, &server->readfds, NULL, NULL, NULL);
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    activity = select(max_sd + 1, &server->readfds, NULL, NULL, &timeout);
    // while(activity == 0) {
    //     activity = select(max_sd + 1, &server->readfds, NULL, NULL, &timeout);
    //     if (activity == 0) {
    //         printf("timeout\n");
    //     }
    // }
    if ((activity < 0) && (errno != EINTR)) {
        printf("select error");
    }
    //If something happened on the master socket , then its an incoming connection
    if (FD_ISSET(server->serverSocket, &server->readfds)) {
        struct sockaddr_in remoteIAddr;
        struct sockaddr *remoteAddr = (struct sockaddr *)&remoteIAddr;
        socklen_t addSize;
        SOCKET new_socket = accept(server->serverSocket, remoteAddr, &addSize);
        if (new_socket < 0)
            errx(EXIT_FAILURE, "ERROR on accept. At "__FILE__":%d\n", __LINE__);
        //inform user of socket number - used in send and receive commands
        fprintf(stderr, "New connection , socket fd is %d , ip is : %s , port : %d \n",
            new_socket, inet_ntoa(remoteIAddr.sin_addr), ntohs(remoteIAddr.sin_port));

        //send new connection greeting message
        char *message = "MFOG Daemon v1.0 \r\n";
        if (send(new_socket, message, strlen(message), 0) != strlen(message)) {
            perror("send");
        }
        // puts("Welcome message sent successfully");

        //add new socket to array of sockets
        for (int i = 0; i < server->clientsLen; i++) {
            //if position is empty
            if (server->clients[i] == 0) {
                server->clients[i] = new_socket;
                printf("Adding to list of sockets as %d\n", i);
                break;
            }
        }
    }
    return activity;
    /*
    //else its some IO operation on some other socket :)
    for (size_t i = 0; i < server->clientsLen; i++) {
        int sd = server->clients[i];

        if (FD_ISSET(sd, server->readfds)) {
            char buffer[1025]; //data buffer of 1K
            //Check if it was for closing , and also read the incoming message
            ssize_t valread = read(sd, buffer, 1024);
            if (valread == 0) {
                //Somebody disconnected , get his details and print
                getpeername(sd, server->addr, server->addrlen);
                printf("Host disconnected , ip %s , port %d \n",
                       inet_ntoa(server->address.sin_addr), ntohs(server->address.sin_port));

                //Close the socket and mark as 0 in list for reuse
                close(sd);
                server->clients[i] = 0;
            }
            //Echo back the message that came in
            else {
                //set the string terminating NULL byte on the end of the data read
                buffer[valread] = '\0';
                send(sd, buffer, strlen(buffer), 0);
            }
        }
    }
    */
}

int serverRead(SOCKET connectionFD) {
    char buffer[256];
    bzero(buffer, 256);
    int n = read(connectionFD, buffer, 255);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR reading from socket. At "__FILE__":%d\n", __LINE__);
    printf("Here is the message: %s\n", buffer);
    n = write(connectionFD, "I got your message", 18);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket. At "__FILE__":%d\n", __LINE__);
    // close(connectionFD);
    // close(sockfd);
    return n;
}

int serverMain(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        errx(EXIT_FAILURE, "ERROR opening socket. At "__FILE__":%d\n", __LINE__);
    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errx(EXIT_FAILURE, "ERROR on binding. At "__FILE__":%d\n", __LINE__);
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0)
        errx(EXIT_FAILURE, "ERROR on accept. At "__FILE__":%d\n", __LINE__);
    bzero(buffer, 256);
    n = read(newsockfd, buffer, 255);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR reading from socket. At "__FILE__":%d\n", __LINE__);
    printf("Here is the message: %s\n", buffer);
    n = write(newsockfd, "I got your message", 18);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket. At "__FILE__":%d\n", __LINE__);
    close(newsockfd);
    close(sockfd);
    return 0;
}

SOCKET clientConnect(char hostname[], short unsigned int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        errx(EXIT_FAILURE, "ERROR opening socket. At "__FILE__":%d\n", __LINE__);
    struct hostent *server = gethostbyname(hostname);
    if (server == NULL)
        errx(EXIT_FAILURE, "ERROR, no such host called '%s'. At "__FILE__":%d\n", hostname, __LINE__);
    struct sockaddr_in serv_addr;
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errx(EXIT_FAILURE, "ERROR connecting. At "__FILE__":%d\n", __LINE__);
    fprintf(stderr, "clientConnect\n");
    return sockfd;
}

int clientRead(SOCKET sockfd) {
    char buffer[256];
    printf("Please enter the message: ");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);
    int n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket. At "__FILE__":%d\n", __LINE__);
    bzero(buffer, 256);
    n = read(sockfd, buffer, 255);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR reading from socket. At "__FILE__":%d\n", __LINE__);
    printf("%s\n", buffer);
    close(sockfd);
    return 0;
}

int client(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        errx(EXIT_FAILURE, "ERROR opening socket. At "__FILE__":%d\n", __LINE__);
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errx(EXIT_FAILURE, "ERROR connecting. At "__FILE__":%d\n", __LINE__);
    printf("Please enter the message: ");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket. At "__FILE__":%d\n", __LINE__);
    bzero(buffer, 256);
    n = read(sockfd, buffer, 255);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR reading from socket. At "__FILE__":%d\n", __LINE__);
    printf("%s\n", buffer);
    close(sockfd);
    return 0;
}

#endif // _MFOG_NET_C

// #ifndef MAIN
// #define MAIN

// int main(int argc, char *argv[]) {
//     if (argc == 2) {
//         server(argc, argv);
//     } else {
//         client(argc, argv);
//     }
//     return 0;
// }

// #endif // MAIN
