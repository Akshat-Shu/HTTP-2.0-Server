#include "client.h"
#include "Utils/Logger/logger.h"

std::string Client::getIp(const sockaddr_in6& addr) {
    char ipStr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr.sin6_addr, ipStr, sizeof(ipStr));
    return std::string(ipStr);
}

Client::Client(int id, int fd, const sockaddr_in6& addr, socklen_t addrLen, SSL* ssl, WebBinder* binder)
    : id(id), addr(addr), addrLen(addrLen), start(time(nullptr)),
    errorCode(0), clientFD(fd, EpollFdType::CLIENT), lastProcessedStream(-1),
    recvBuffer(BUFFER_SIZE), ssl(ssl), binder(binder) {
    ip = getIp(addr);
    state = State::HANDSHAKE;

    hpackDecoder = std::make_unique<http2::protocol::hpack::Decoder>();
}

bool Client::isTimedOut(const int timeout) const {
    return difftime(time(nullptr), start) > timeout;
}

bool Client::sendData(const std::vector<uint8_t>& data, int weight) {
    Logger::debug("Data: " + toHex(data.data(), data.size()) +
                  ", size: " + std::to_string(data.size()) +
                  ", for client ID: " + std::to_string(id));

    state = WRITING;
    auto bytesSent = threadPool->enqueue(weight, [this, data]() {
        ssize_t bytesSent = SSL_write(ssl, data.data(), data.size());
        if (bytesSent < 0) {
            Logger::error("Error sending frame to client: " + std::to_string(errno));
            return -1;
        } else if (bytesSent < static_cast<ssize_t>(data.size())) {
            Logger::warning("Partial frame sent to client, expected: " + std::to_string(data.size()) +
                            ", sent: " + std::to_string(bytesSent));
        } else {
            Logger::debug("Sent frame to client ID: " + std::to_string(id));
        }
        return (int) bytesSent;
    });
    state = CLIENT_IDLE;
    return bytesSent.get() >= 0;
}

bool Client::sendFrame(const http2::protocol::Frame& frame, int weight) {
    std::vector<uint8_t> encodedFrame = frame.encode();

    return sendData(encodedFrame, weight);
}

bool Client::ackSettings(const http2::protocol::Frame& frame) {
    http2::protocol::Settings settings;
    http2::protocol::Error err = settings.decode(frame.payload());

    if (err != http2::protocol::NO_ERROR) {
        Logger::error("Error decoding settings frame for client ID: " + std::to_string(id) +
                      ", error code: " + std::to_string(err));
        return false;
    }


    Logger::debug("Received settings from client ID: " + std::to_string(id) +
                 ", Header Table Size: " + std::to_string(settings.header_table_size()) +
                 ", Enable Push: " + std::to_string(settings.enable_push()) +
                 ", Max Concurrent Streams: " + std::to_string(settings.max_concurrent_streams()) +
                 ", Initial Window Size: " + std::to_string(settings.initial_window_size()) +
                 ", Max Frame Size: " + std::to_string(settings.max_frame_size()) +
                 ", Max Header List Size: " + std::to_string(settings.max_header_list_size()));

    http2::protocol::Frame settingsAck(
        http2::protocol::SETTINGS_FRAME,
        http2::protocol::ACK,
        0
    );

    settingsAck.mutable_payload().resize(0);

    this->settings = settings;    
    sendFrame(settingsAck);

    return true;
}

bool Client::applySettings() {
    http2::protocol::Settings settigns;
    vector<uint8_t> encodedSettings;
    settigns.encode(encodedSettings);

    http2::protocol::Frame settingsFrame(
        http2::protocol::SETTINGS_FRAME,
        http2::protocol::NO_FLAGS,
        0
    );

    settingsFrame.mutable_payload() = std::move(encodedSettings);

    if (!sendFrame(settingsFrame)) {
        Logger::error("Failed to send settings frame to client ID: " + std::to_string(id));
        return false;
    }

    return true;
}

void Client::doRequest(epoll_event& event) {
    recvBuffer.resize(BUFFER_SIZE);
    // ssize_t bytesRead = recv(clientFD.fd, recvBuffer.data(), recvBuffer.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
    state = READING;
    ssize_t bytesRead = SSL_read(ssl, recvBuffer.data(), recvBuffer.size());
    state = CLIENT_IDLE;
    // Logger::debug("Client ID: " + std::to_string(id) + " received data, bytes read: " + std::to_string(bytesRead));

    if (bytesRead < 0) {
        // Logger::error("Error reading from client ID: " + std::to_string(id) + 
                    //   ", error code: " + std::to_string(errno));
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        } else {
            errorCode = errno;
            Logger::error("Error reading from client: " + std::to_string(errorCode));
            return;
        }
    } else if (bytesRead == 0) {
        Logger::error("Client closed connection");
        clientFD.setState(FdState::FD_CLOSED);
        return;
    }

    Logger::debug("Client ID: " + std::to_string(id) + " received data, bytes read: " + std::to_string(bytesRead));
    Logger::debug("Data: " + toHex(recvBuffer.data(), bytesRead, 512));

    std::string prefaceStr = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    if (recvBuffer.size() >= prefaceStr.size() && 
        std::equal(prefaceStr.begin(), prefaceStr.end(), recvBuffer.begin())) {
        Logger::info("HTTP/2 preface received from client ID: " + std::to_string(id));
        applySettings();
        recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + prefaceStr.size());
        bytesRead -= prefaceStr.size();
    }

    std::vector<http2::protocol::Frame> frames = http2::protocol::Frame::toFrames(
        recvBuffer.data(),
        recvBuffer.data() + bytesRead);

    Logger::info("Received " + std::to_string(frames.size()) + " frames from client ID: " + std::to_string(id));

    for(const auto& frame: frames) {
        if(frame.isPriority()) {
            Logger::debug("Priority frame received for stream ID: " + std::to_string(frame.stream_id()));
        }
        if(FrameHandler::handleFrame(this, frame)) {
            Logger::info("Handled frame of type: " + std::to_string(frame.type()) + 
                         " for client ID: " + std::to_string(id));
        } else {
            Logger::error("Error handling frame of type: " + std::to_string(frame.type()) + 
                          " for client ID: " + std::to_string(id));
        }
    }
}

int Client::continueHandshake() {
    if(state != State::HANDSHAKE) return 1;

    int ret = SSL_accept(ssl);

    if(ret > 0) {
        const unsigned char* alpn = NULL;
        unsigned int alpnLen = 0;
        SSL_get0_alpn_selected(ssl, &alpn, &alpnLen);
        
        if (alpnLen == 2 && alpn[0] == 'h' && alpn[1] == '2') {
            Logger::info("HTTP/2 negotiated via ALPN for client ID: " + std::to_string(id));
            return 1;
        } else {
            Logger::error("HTTP/2 not negotiated via ALPN for client ID: " + std::to_string(id));
            SSL_free(ssl);
            state = State::CLIENT_CLOSED;
            return -1;
        }
    } else {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            // Logger::debug("SSL_accept wants more data for client ID: " + std::to_string(id));
            return 0;
        } else {
            ERR_print_errors_fp(stdout);
            Logger::error("SSL_accept failed for client ID: " + std::to_string(id) + 
                          ", error code: " + std::to_string(err));
            SSL_free(ssl);
            state = State::CLIENT_CLOSED;
            return -1;
        }
    }
}