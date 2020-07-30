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
#include <netinet/in.h>
#include <netdb.h>
#include <err.h>

int serverStart(short unsigned int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        errx(EXIT_FAILURE, "ERROR opening socket");
    struct sockaddr_in serv_addr;
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0)
        errx(EXIT_FAILURE, "ERROR on binding");
    listen(sockfd, 5);
    return sockfd;
}

int serverAccept(int sockfd) {
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0)
        errx(EXIT_FAILURE, "ERROR on accept");
    return newsockfd;
}

int serverRead(int connectionFD) {
    char buffer[256];
    bzero(buffer, 256);
    int n = read(connectionFD, buffer, 255);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR reading from socket");
    printf("Here is the message: %s\n", buffer);
    n = write(connectionFD, "I got your message", 18);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket");
    // close(connectionFD);
    // close(sockfd);
    return n;
}

int server(int argc, char *argv[]) {
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
        errx(EXIT_FAILURE, "ERROR opening socket");
    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errx(EXIT_FAILURE, "ERROR on binding");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0)
        errx(EXIT_FAILURE, "ERROR on accept");
    bzero(buffer, 256);
    n = read(newsockfd, buffer, 255);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR reading from socket");
    printf("Here is the message: %s\n", buffer);
    n = write(newsockfd, "I got your message", 18);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket");
    close(newsockfd);
    close(sockfd);
    return 0;
}

int clientConnect(char hostname[], short unsigned int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        errx(EXIT_FAILURE, "ERROR opening socket");
    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    struct sockaddr_in serv_addr;
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errx(EXIT_FAILURE, "ERROR connecting");
    return sockfd;
}

int clientRead(int sockfd) {
    char buffer[256];
    printf("Please enter the message: ");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);
    int n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket");
    bzero(buffer, 256);
    n = read(sockfd, buffer, 255);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR reading from socket");
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
        errx(EXIT_FAILURE, "ERROR opening socket");
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
        errx(EXIT_FAILURE, "ERROR connecting");
    printf("Please enter the message: ");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR writing to socket");
    bzero(buffer, 256);
    n = read(sockfd, buffer, 255);
    if (n < 0)
        errx(EXIT_FAILURE, "ERROR reading from socket");
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
