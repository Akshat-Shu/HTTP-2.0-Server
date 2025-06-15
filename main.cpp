#include "Client/clientManager.h"
#include "Networking/Epoller/epoller.h"
#include "Networking/Socket/socket.h"
#include "Utils/Logger/logger.h"
#include "atomic"

bool running = true;

int main() {
    Epoller epoller;
    // ClientManager clientManager;

    Socket socket(8080);

    epoller.addFD(socket.sockFD);

    while(running) {
        epoller.epollLoop();
    }
}