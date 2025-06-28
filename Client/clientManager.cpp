#include "clientManager.h"
#include "Utils/Logger/logger.h"
#include "fcntl.h"

int ClientManager::ctr = 0;
std::map<int, Client*> ClientManager::clients;
ThreadPool ClientManager::threadPool = ThreadPool(4);


Client* ClientManager::acceptClient(int socket, SSL_CTX* ctx, WebBinder* binder) {
    struct sockaddr addr;
    socklen_t addrLen = sizeof(addr);
    
    int clientFD = accept(socket, &addr, &addrLen);

    if(clientFD < 0) {
        Logger::error("Failed to accept client connection: " + std::to_string(errno));
        return nullptr;
    }

    int flags = fcntl(clientFD, F_GETFL, 0);
    if (flags < 0) {
        Logger::error("Failed to get flags for client FD: " + std::to_string(clientFD));
        close(clientFD);
        return nullptr;
    }

    flags |= O_NONBLOCK;
    if (fcntl(clientFD, F_SETFL, flags) < 0) {
        Logger::error("Failed to set non-blocking mode for client FD: " + std::to_string(clientFD));
        close(clientFD);
        return nullptr;
    }

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientFD);
    SSL_set_mode(ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE);
        
    // if (SSL_accept(ssl) <= 0) {
    //     ERR_print_errors_fp(stdout);
    //     Logger::error("SSL_accept failed for client FD: " + std::to_string(clientFD));
    //     SSL_free(ssl);
    //     close(clientFD);
    //     return nullptr;
    // }
    
    // const unsigned char* alpn = NULL;
    // unsigned int alpnLen = 0;
    // SSL_get0_alpn_selected(ssl, &alpn, &alpnLen);
    
    // if (alpnLen == 2 && alpn[0] == 'h' && alpn[1] == '2') {
    //     Logger::info("HTTP/2 negotiated via ALPN for client FD: " + std::to_string(clientFD));
    // } else {
    //     Logger::error("HTTP/2 not negotiated via ALPN for client FD: " + std::to_string(clientFD));
    //     SSL_free(ssl);
    //     close(clientFD);
    //     return nullptr;
    // } -> shift all this to be non blocking

    Client* client = new Client(++ctr, clientFD, *reinterpret_cast<sockaddr_in6*>(&addr), addrLen, &threadPool, ssl, binder);
    clients[clientFD] = client;

    Logger::info("Accepted new client with ID: " + std::to_string(client->id) + 
                 " from IP: " + client->ip + 
                 " on socket with FD: " + std::to_string(socket));

    // if(!client->sendUpgradeHeader()) { // no update header for HTTP/2
    //     Logger::error("Failed to send upgrade header to client ID: " + std::to_string(client->id));
    //     removeClient(client->id);
    //     return nullptr;
    // }

    // if (!client->sendPreface()) {
    //     Logger::error("Failed to send preface to client ID: " + std::to_string(client->id));
    //     removeClient(client->id);
    //     return nullptr;
    // }

    // if(!client->applySettings()) {
    //     Logger::error("Failed to apply settings for client ID: " + std::to_string(client->id));
    //     removeClient(client->id);
    //     return nullptr;
    // }

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

    if(client->state == State::CLIENT_CLOSED) {
        Logger::info("Client ID: " + std::to_string(client->id) + " is closed, removing from epoll.");
        removeClient(client->id);
        return;
    }

    try {
        if(client->state == HANDSHAKE) {
            int val = client->continueHandshake();
            if(val == 1) {
                Logger::info("Client ID: " + std::to_string(client->id) + " handshake completed.");
                client->state = State::CLIENT_IDLE;
            } else if(val < 0) {
                Logger::error("Error during handshake for client ID: " + std::to_string(client->id));
                removeClient(client->id);
                return;
            } else {
                // Logger::debug("Continuing handshake for client ID: " + std::to_string(client->id));
                return;
            }
        }

        if(client->state != State::CLIENT_IDLE) {
            Logger::debug("Client ID: " + std::to_string(client->id) + " is not idle, skipping request handling.");
            return;
        }
        client->doRequest(event);
    } catch (const std::exception& e) {
        Logger::error("Error handling client ID: " + std::to_string(client->id) + " - " + e.what());
        removeClient(client->id);
    }
}