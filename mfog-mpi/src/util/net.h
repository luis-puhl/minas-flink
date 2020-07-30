#ifndef _MFOG_NET_H
#define _MFOG_NET_H

// int server(int argc, char *argv[]);
int serverStart(int port);
int serverAccept(int sockfd);
int serverRead(int connectionFD);

int clientConnect(char hostname[], short unsigned int port);
int clientRead(int sockfd);

#endif // _MFOG_NET_H