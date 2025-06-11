#include <string>
#include <arpa/inet.h>
#include "time.h"
#include <vector>

class Client {
public:
    int id;
    sockaddr_in6 addr;
    socklen_t addrLen;
    time_t start;
    std::string ip;
    int errorCode;
    int pid;

    static std::vector<Client*> clients;
    static int ctr;


};