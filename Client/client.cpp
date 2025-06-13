#include "client.h"
#include "Utils/Logger/logger.h"

static std::string getIp(const sockaddr_in6& addr) {
    char ipStr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr.sin6_addr, ipStr, sizeof(ipStr));
    return std::string(ipStr);
}

Client::Client(int id, int fd, const sockaddr_in6& addr, socklen_t addrLen)
    : id(id), addr(addr), addrLen(addrLen), start(time(nullptr)),
    errorCode(0), clientFD(fd, EpollFdType::CLIENT) {
    ip = getIp(addr);
}

bool Client::isTimedOut(const int timeout) const {
    return difftime(time(nullptr), start) > timeout;
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