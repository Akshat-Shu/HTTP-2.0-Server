#include "Client/clientManager.h"
#include "Networking/Epoller/epoller.h"
#include "Networking/Socket/socket.h"
#include "Utils/Logger/logger.h"
#include "atomic"
#include "WebBinder/webBinder.h"

bool running = true;

int main() {
    Epoller epoller;
    // ClientManager clientManager;

    Socket socket(8080);

    WebBinder webBinder;

    webBinder.bindDirectory("./html/", "/html");
    webBinder.bindFile("./html/reallyCoolSite.html", "/");

    epoller.setBinder(webBinder);

    epoller.addFD(socket.sockFD);

    while(running) {
        epoller.epollLoop();
    }
}