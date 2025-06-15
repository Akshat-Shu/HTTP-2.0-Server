#include <netinet/in.h>
#include <vector>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT 8080
#define MAX_QUEUE 5

#pragma once

class Socket {
public:
    int port;
    int id;
    int sockFD;
    sockaddr_in6 addr;
    socklen_t addrLen;
    SSL_CTX* ctx;

    static std::vector<Socket*> sockets;
    static int ctr;

    const static unsigned char alpnProtocol[];

    static int alpnSelectCb(SSL *ssl, const unsigned char **out, 
                            unsigned char *outlen, const unsigned char *in, 
                            unsigned int inlen, void *arg); 

    Socket(int port);

    Socket(): Socket(PORT) {}

};