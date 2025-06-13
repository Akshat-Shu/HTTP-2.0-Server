#include <string>
#include <arpa/inet.h>
#include "time.h"
#include "Networking/Epoller/fileDescriptor.h"
#include <vector>
#include <sys/epoll.h>
#include "http2/protocol/frame.h"

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

    bool isTimedOut(const int timeout=TIMEOUT) const;

    void doRequest(epoll_event& event);
};