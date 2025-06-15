#include "client.h"
#include "Utils/Logger/logger.h"

static std::string getIp(const sockaddr_in6& addr) {
    char ipStr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr.sin6_addr, ipStr, sizeof(ipStr));
    return std::string(ipStr);
}

Client::Client(int id, int fd, const sockaddr_in6& addr, socklen_t addrLen)
    : id(id), addr(addr), addrLen(addrLen), start(time(nullptr)),
    errorCode(0), clientFD(fd, EpollFdType::CLIENT), lastProcessedStream(-1) {
    ip = getIp(addr);
}

bool Client::isTimedOut(const int timeout) const {
    return difftime(time(nullptr), start) > timeout;
}

bool Client::sendFrame(const http2::protocol::Frame& frame) {
    std::vector<uint8_t> encodedFrame = frame.encode();
    ssize_t bytesSent = send(clientFD.fd, encodedFrame.data(), encodedFrame.size(), MSG_DONTWAIT | MSG_NOSIGNAL);

    if (bytesSent < 0) {
        errorCode = errno;
        Logger::error("Error sending frame to client: " + std::to_string(errorCode));
        return false;
    } else if (bytesSent < static_cast<ssize_t>(encodedFrame.size())) {
        Logger::warning("Partial frame sent to client, expected: " + std::to_string(encodedFrame.size()) +
                        ", sent: " + std::to_string(bytesSent));
    }

    return true;
}

bool Client::sendPreface() {
    std::string preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    ssize_t bytesSent = send(clientFD.fd, preface.c_str(), preface.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
    
    if (bytesSent < 0) {
        errorCode = errno;
        Logger::error("Error sending preface to client: " + std::to_string(errorCode));
        return false;
    } else if (bytesSent < static_cast<ssize_t>(preface.size())) {
        Logger::warning("Partial preface sent to client, expected: " + std::to_string(preface.size()) +
                        ", sent: " + std::to_string(bytesSent));
        
    }
    
    return bytesSent == static_cast<ssize_t>(preface.size());
}

void Client::doRequest(epoll_event& event) {
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = recv(clientFD.fd, buffer, sizeof(buffer), MSG_DONTWAIT | MSG_NOSIGNAL);

    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        } else {
            errorCode = errno;
            Logger::error("Error reading from client: " + std::to_string(errorCode));
        }
    } else if (bytesRead == 0) {
        Logger::error("Client closed connection");
    }

}