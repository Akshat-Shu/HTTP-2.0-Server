#include <string>
#include <arpa/inet.h>
#include "time.h"
#include "Networking/Epoller/fileDescriptor.h"
#include <vector>
#include <sys/epoll.h>
#include "http2/protocol/hpack/hpack.h"
#include "http2/protocol/frame.h"
#include "http2/protocol/settings.h"
#include "http2/protocol/error.h"
#include "http2/headers/headers.h"
#include "Multithreading/threadPool.h"
#include "stream.h"
#include <openssl/ssl.h>
#include "Utils/toHex.cpp"
#include "WebBinder/webBinder.h"
#include "Response/response.h"
#include "Frame-Handler/frameHandler.h"

#pragma once

#define TIMEOUT 600
#define BUFFER_SIZE 4096

class Client {
public:
    int id;
    sockaddr_in6 addr;
    socklen_t addrLen;
    time_t start;
    std::string ip;
    int errorCode;
    Fd clientFD;
    std::map<int, Stream*> streams;
    int lastProcessedStream;
    ThreadPool* threadPool;
    std::vector<uint8_t> recvBuffer;
    SSL* ssl;
    WebBinder* binder;
    http2::protocol::Settings settings;

    // static std::map<int, Client*> clients;
    // static int ctr;

    struct findById {
        int id_find;

        bool operator()(const Client& c) const {
            return c.id == id_find;
        }

        bool operator()(const int& id) const {
            return id == id_find;
        }

        findById(int id) : id_find(id) {}
    };


    static std::string getIp(const sockaddr_in6& addr); 

    Client(int id, int fd, const sockaddr_in6& addr, socklen_t addrLen, SSL* ssl, WebBinder* binder);

    Client(int id, int fd, const sockaddr_in6& addr, socklen_t addrLen, ThreadPool* thPool, SSL* ssl, WebBinder* binder)
        : Client(id, fd, addr, addrLen, ssl, binder) {
        threadPool = thPool;
    }

    bool isTimedOut(const int timeout=TIMEOUT) const;

    void doRequest(epoll_event& event);

    bool sendFrame(const http2::protocol::Frame& frame, int weight = 0);

    bool sendData(const std::vector<uint8_t>& data, int weight = 0);

    bool acceptPreface();

    bool ackSettings(const http2::protocol::Frame& frame);

    bool applySettings();

};