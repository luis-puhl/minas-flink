#ifndef _MFOG_NET_H
#define _MFOG_NET_H 1

#include <sys/socket.h>
#include <netinet/in.h>

typedef int SOCKET;

typedef struct net_server {
    SOCKET serverSocket, *clients;
    int clientsLen;
    fd_set readfds;
} server_t;

// int server(int argc, char *argv[]);
server_t *serverStart(short unsigned int port);
SOCKET serverAccept(SOCKET sockfd);
void serverSelect(struct net_server *server);
void serverDisconnect(server_t *server, SOCKET connection, int connectionIndex);
int serverRead(SOCKET connectionFD);

SOCKET clientConnect(char hostname[], short unsigned int port);
int clientRead(SOCKET sockfd);

#endif // _MFOG_NET_H