#include "clientManager.h"
#include "Utils/Logger/logger.h"

int ClientManager::ctr = 0;
std::map<int, Client*> ClientManager::clients;
ThreadPool ClientManager::threadPool = ThreadPool(4);


Client* ClientManager::acceptClient(int socket) {
    struct sockaddr addr;
    socklen_t addrLen = sizeof(addr);
    
    int clientFD = accept(socket, &addr, &addrLen);

    if(clientFD < 0) {
        Logger::error("Failed to accept client connection: " + std::to_string(errno));
        return;
    }

    Client* client = new Client(++ctr, clientFD, *reinterpret_cast<sockaddr_in6*>(&addr), addrLen);
    clients[clientFD] = client;

    Logger::info("Accepted new client with ID: " + std::to_string(client->id) + 
                 " from IP: " + client->ip + 
                 " on socket with FD: " + std::to_string(socket));

    if (!client->sendPreface()) {
        Logger::error("Failed to send preface to client ID: " + std::to_string(client->id));
        removeClient(client->id);
        return nullptr;
    }

    return client;
}

void ClientManager::clientLoop() {
    for (auto& pair : clients) {
        Client* client = pair.second;
        if (client->isTimedOut()) {
            Logger::warning("Client ID: " + std::to_string(client->id) + " timed out.");
            removeClient(client->id);
        }
    }
}

void ClientManager::handleClient(Client* client, epoll_event& event) {
    if (client->isTimedOut()) {
        Logger::warning("Client ID: " + std::to_string(client->id) + " timed out.");
        removeClient(client->id);
        return;
    }

    try {
        client->doRequest(event);
    } catch (const std::exception& e) {
        Logger::error("Error handling client ID: " + std::to_string(client->id) + " - " + e.what());
        removeClient(client->id);
    }
}