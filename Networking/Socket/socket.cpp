#include "socket.h"
#include "Utils/Logger/logger.h"
#include <unistd.h>
#include "fcntl.h"

int Socket::ctr = 0;
std::vector<Socket*> Socket::sockets;
const unsigned char Socket::alpnProtocol[] = {
    2, 'h', '2',
    // decided to not support HTTP/1.1 sicne all browsers support HTTP/2
    // 8, 'h', 't', 't', 'p', '/', '1', '.', '1'
};

Socket::Socket(int port) : port(port), id(ctr++), sockFD(-1) {

    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        Logger::error("Failed to create SSL context");
        ERR_print_errors_fp(stdout);
        return;
    }

    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
        Logger::error("Failed to load certificate");
        ERR_print_errors_fp(stdout);
        return;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        Logger::error("Failed to load private key");
        ERR_print_errors_fp(stdout);
        return;
    }

    SSL_CTX_set_alpn_select_cb(ctx, alpnSelectCb, NULL);


    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;
    addrLen = sizeof(addr);
    
    sockFD = socket(AF_INET6, SOCK_STREAM, 0);
    
    if (sockFD < 0) {
        Logger::error("Failed to create socket");
        close(sockFD);
        return;
    }

    int opt = 1;
    if(setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        // so that we can run on the same port without waiting
        Logger::error("Failed to set socket options");
        close(sockFD);
        return;
    }

    if (bind(sockFD, (struct sockaddr*)&addr, addrLen) < 0) {
        Logger::error("Failed to bind socket");
        close(sockFD);
        return;
    }

    if (listen(sockFD, MAX_QUEUE) < 0) {
        Logger::error("Failed to listen on socket");
        close(sockFD);
        return;
    }

    int flags = fcntl(sockFD, F_GETFL, O_NONBLOCK | O_CLOEXEC);
    
    if (flags == -1) {
        Logger::error("Failed to get socket flags");
        close(sockFD);
        return;
    }


    Logger::info("Socket created and listening on port " + std::to_string(port));
    sockets.push_back(this);
}


int Socket::alpnSelectCb(SSL *ssl, const unsigned char **out, 
                                    unsigned char *outlen, const unsigned char *in, 
                                    unsigned int inlen, void *arg) {
    if (SSL_select_next_proto((unsigned char**)out, outlen, 
                            (unsigned char*)alpnProtocol, sizeof(alpnProtocol), 
                            in, inlen) != OPENSSL_NPN_NEGOTIATED) {
        return SSL_TLSEXT_ERR_NOACK;
    }
    
    if (*outlen == 2 && (*out)[0] == 'h' && (*out)[1] == '2') {
        return SSL_TLSEXT_ERR_OK;
    }
    
    return SSL_TLSEXT_ERR_ALERT_WARNING;
}