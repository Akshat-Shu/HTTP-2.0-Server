#include "epoller.h"
#include "Utils/Logger/logger.h"
#include <algorithm>


Epoller::Epoller() {
    events = std::vector<struct epoll_event>(MAX_EVENTS);
    epollFD = epoll_create1(0);

    if (epollFD == -1) {
        Logger::fatal("Failed to create epoll file descriptor");
    }
    Logger::info("Epoll file descriptor created with ID: " + std::to_string(epollFD));
}

bool Epoller::addFD(int fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLOUT;

    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &event) == -1) {
        Logger::error("Failed to add file descriptor to epoll: " + std::to_string(fd));
        return false;
    }

    Logger::info("Added file descriptor to epoll: " + std::to_string(fd));
    return true;
}

bool Epoller::addFD(int fd, Client* client) {
    bool result = addFD(fd);

    if(!result) {
        Logger::error("Failed to add file descriptor for client with ID: " + std::to_string(client->id));
        return false;
    }

    Logger::info("Added file descriptor for client with ID: " + std::to_string(client->id));
    return true;
}

bool Epoller::removeFD(int fd) {
    if(epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, events.data()) == -1) {
        Logger::error("Failed to remove file descriptor to epoll: " + std::to_string(fd));
        return false;
    }

    Logger::info("Removed file decriptor to epoll: " + std::to_string(fd));
    return true;
}

bool Epoller::removeFD(int fd, Client* client) {
    bool result = removeFD(fd);
    if(!result) {
        Logger::error("Faield to remove file descriptor for client with ID: " + std::to_string(client->id));
        return false;
    }

    Logger::info("Removed file descriptor for client with ID: " + std::to_string(client->id));
    return true;
}

EpollFdType Epoller::getType(int fd) {
    struct Fd::find functor = Fd::find(fd);
    auto it = std::find_if(ClientManager::clients.begin(), ClientManager::clients.end(),
                           [&functor](const std::pair<int, Client*>& pair) {
                               return functor(pair.second->clientFD);
                           });

    if(it != ClientManager::clients.end()) {
        return CLIENT;
    }

    auto it1 = std::find_if(Socket::sockets.begin(), Socket::sockets.end(),
                      [&functor](Socket* socket) {
                          return functor(socket->sockFD);
                      });

    if(it1 != Socket::sockets.end()) {
        return SOCKET;
    }

    if(functor(epollFD)) return SERVER;

    return NONE;
}


void Epoller::epollLoop() {

    int nfd = epoll_wait(epollFD, events.data(), MAX_EVENTS, -1);

    if (nfd == -1) {
        Logger::error("Epoll wait failed");
        return;
    }

    // if(nfd > 0) {
    //     Logger::debug("Epoll wait returned " + std::to_string(nfd) + " events");
    // }

    for(int i = 0; i < nfd; ++i) {
        int fd = events[i].data.fd;
        EpollFdType type = getType(fd);

        if (type == CLIENT) {
            auto it = ClientManager::clients.find(fd);
            if (it != ClientManager::clients.end()) {
                Client* client = it->second;
                ClientManager::handleClient(client, events[i]);
                // Logger::info("Handling client event for client ID: " + std::to_string(client->id));
                
                if(client->clientFD.state == FD_CLOSED) {
                    Logger::info("Client with ID " + std::to_string(client->id) + " is closed, removing from epoll");
                    removeFD(client->clientFD.fd, client);
                    ClientManager::removeClient(client->id);
                }
                
            } else {
                Logger::warning("Unknown Client with FD " + std::to_string(fd));
            }
        } else if (type == SOCKET) {
            auto it1 = std::find_if(Socket::sockets.begin(), Socket::sockets.end(),
                                    [fd](Socket* socket) { return socket->sockFD == fd; });
            if (it1 != Socket::sockets.end()) {
                Socket* socket = *it1;
                Logger::info("Handling socket event for socket ID: " + std::to_string(socket->id));
                Client* newClient = ClientManager::acceptClient(socket->sockFD, socket->ctx, binder);
                addFD(newClient->clientFD.fd);
            } else {
                Logger::warning("Unknown Socket with FD " + std::to_string(fd));
            }
        } else if (type == SERVER) {
            Logger::info("Server event for epoll FD: " + std::to_string(epollFD));
        } else {
            Logger::warning("Unknown file descriptor type for FD: " + std::to_string(fd));
        }
    }
}