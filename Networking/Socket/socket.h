#include <netinet/in.h>
#include <vector>

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

    static std::vector<Socket*> sockets;
    static int ctr;

    Socket(int port);

    Socket(): Socket(PORT) {}

};