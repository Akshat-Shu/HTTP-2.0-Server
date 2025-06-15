#include <sys/epoll.h>
#include <vector>
#include "Client/client.h"
#include "Networking/Socket/socket.h"
#include "Client/client.h"
#include "Client/clientManager.h"
#include "Networking/Epoller/fileDescriptor.h"
#include <unistd.h>

#pragma once
#define MAX_EVENTS 100



class Epoller {
public:
    int epollFD;
    std::vector<struct epoll_event> events;

    bool addFD(int fd);
    bool addFD(int fd, Client* client);
    bool removeFD(int fd);
    bool removeFD(int fd, Client* client);

    EpollFdType getType(int fd);

    Epoller();

    ~Epoller() {
        if (epollFD != -1) {
            close(epollFD);
        }
    }

    void epollLoop();
};