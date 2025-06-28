#pragma once

#include <map>
#include <netinet/in.h>
#include "client.h"
#include "Networking/Epoller/fileDescriptor.h"
#include <algorithm>
#include "Networking/Socket/socket.h"
#include <sys/epoll.h>
#include "Multithreading/threadPool.h"
#include <openssl/ssl.h>
#include "unistd.h"

class ClientManager {
public:
    static std::map<int, Client*> clients;
    static int ctr;

    static ThreadPool threadPool;

    static Client* getClient(int id) {
        auto functor = Client::findById(id);
        auto it = std::find_if(clients.begin(), clients.end(),
                               [&functor](const std::pair<int, Client*>& pair) {
                                   return functor(*pair.second);
                               });

        if (it != clients.end()) {
            return it->second;
        }
        return nullptr;
    }

    static Client* acceptClient(int socket, SSL_CTX* ctx, WebBinder* binder);

    static void removeClient(Fd fd) {
        auto it = clients.find(fd.fd);
        if (it != clients.end()) {
            delete it->second;
            clients.erase(it);
        }
    }

    static void removeClient(int id) {
        auto functor = Client::findById(id);
        auto it = std::find_if(clients.begin(), clients.end(),
                               [&functor](const std::pair<int, Client*>& pair) {
                                   return functor(*pair.second);
                               });

        if (it != clients.end()) {
            Logger::info("Removing client with ID: " + std::to_string(it->second->id));
            it->second->clientFD.setState(FD_CLOSED);
            clients.erase(it);
            delete it->second;
        }
    }

    static void handleClient(Client* client, epoll_event& event);

    static void clientLoop();
};