#include "socket.h"
#include "Utils/Logger/logger.h"
#include <unistd.h>
#include "fcntl.h"

int Socket::ctr = 0;
std::vector<Socket*> Socket::sockets;

Socket::Socket(int port) : port(port), id(ctr++), sockFD(-1) {
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;
    addrLen = sizeof(addr);
    
    sockFD = socket(AF_INET6, SOCK_STREAM, 0);
    
    if (sockFD < 0) {
        Logger::error("Failed to create socket");
        close(sockFD);
        return;
    }

    if(setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
        // so that we can run on the same port without waiting
        Logger::error("Failed to set socket options");
        close(sockFD);
        return;
    }

    if (bind(sockFD, (struct sockaddr*)&addr, addrLen) < 0) {
        Logger::error("Failed to bind socket");
        close(sockFD);
        return;
    }

    if (listen(sockFD, MAX_QUEUE) < 0) {
        Logger::error("Failed to listen on socket");
        close(sockFD);
        return;
    }

    int flags = fcntl(sockFD, F_GETFL, 0);
    if (flags == -1) {
        Logger::error("Failed to get socket flags");
        close(sockFD);
        return;
    }

    if (fcntl(sockFD, F_SETFL, flags | O_NONBLOCK) == -1) {
        Logger::error("Failed to set socket to non-blocking mode");
        close(sockFD);
        return;
    }

    Logger::info("Socket created and listening on port " + std::to_string(port));
    sockets.push_back(this);
}