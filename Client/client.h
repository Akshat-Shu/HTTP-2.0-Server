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
#include "http2/protocol/hpack/hpack.h"
#include "Multithreading/threadPool.h"
#include "stream.h"

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

    Client(int id, int fd, const sockaddr_in6& addr, socklen_t addrLen);

    Client(int id, int fd, const sockaddr_in6& addr, socklen_t addrLen, ThreadPool* thPool)
        : Client(id, fd, addr, addrLen) {
        threadPool = thPool;
    }

    bool isTimedOut(const int timeout=TIMEOUT) const;

    void doRequest(epoll_event& event);

    bool sendFrame(const http2::protocol::Frame& frame);

    bool sendPreface();

    bool applySettings();

    bool sendUpgradeHeader();

    bool handleDataFrame(const http2::protocol::Frame& frame);
    bool handleHeadersFrame(const http2::protocol::Frame& frame);
    bool handlePriorityFrame(const http2::protocol::Frame& frame);
    bool handleResetFrame(const http2::protocol::Frame& frame);
    bool handleSettingsFrame(const http2::protocol::Frame& frame);
    bool handlePushPromiseFrame(const http2::protocol::Frame& frame);
    bool handlePingFrame(const http2::protocol::Frame& frame);
    bool handleGoAwayFrame(const http2::protocol::Frame& frame);
    bool handleWindowUpdateFrame(const http2::protocol::Frame& frame);
    bool handleContinuationFrame(const http2::protocol::Frame& frame);
    bool handleFrame(const http2::protocol::Frame& frame) {
        Logger::debug("Handling frame of type: " + std::to_string(frame.type()) + 
                      " for client ID: " + std::to_string(id));
        switch (frame.type()) {
            case http2::protocol::DATA_FRAME:
                return handleDataFrame(frame);
            case http2::protocol::HEADERS_FRAME:
                return handleHeadersFrame(frame);
            case http2::protocol::PRIORITY_FRAME:
                return handlePriorityFrame(frame);
            case http2::protocol::RST_STREAM_FRAME:
                return handleResetFrame(frame);
            case http2::protocol::SETTINGS_FRAME:
                return handleSettingsFrame(frame);
            case http2::protocol::PUSH_PROMISE_FRAME:
                return handlePushPromiseFrame(frame);
            case http2::protocol::PING_FRAME:
                return handlePingFrame(frame);
            case http2::protocol::GOAWAY_FRAME:
                return handleGoAwayFrame(frame);
            case http2::protocol::WINDOW_UPDATE_FRAME:
                return handleWindowUpdateFrame(frame);
            case http2::protocol::CONTINUATION_FRAME:
                return handleContinuationFrame(frame);
            default:
                Logger::error("Unknown frame type: " + std::to_string(frame.type()));
                return false;
        }
    }

    bool processEndHeader(Stream* stream);
};