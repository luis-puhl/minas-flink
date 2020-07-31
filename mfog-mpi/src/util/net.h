#ifndef _MFOG_NET_H
#define _MFOG_NET_H 1

typedef int SOCKET;

// int server(int argc, char *argv[]);
SOCKET serverStart(short unsigned int port);
SOCKET serverAccept(SOCKET sockfd);
int serverRead(SOCKET connectionFD);

SOCKET clientConnect(char hostname[], short unsigned int port);
int clientRead(SOCKET sockfd);

#endif // _MFOG_NET_H