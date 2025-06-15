#include "client.h"
#include "Utils/Logger/logger.h"

std::string Client::getIp(const sockaddr_in6& addr) {
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
    int fd = clientFD.fd;

    auto bytesSent = threadPool->enqueue([this, encodedFrame, fd]() {
        ssize_t bytesSent = send(fd, encodedFrame.data(), encodedFrame.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
        if (bytesSent < 0) {
            Logger::error("Error sending frame to client: " + std::to_string(errno));
            return -1;
        } else if (bytesSent < static_cast<ssize_t>(encodedFrame.size())) {
            Logger::warning("Partial frame sent to client, expected: " + std::to_string(encodedFrame.size()) +
                            ", sent: " + std::to_string(bytesSent));
        } else {
            Logger::debug("Sent frame to client ID: " + std::to_string(id));
        }
        return (int) bytesSent;
    });

    return bytesSent.get() >= 0;
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
    } else {
        Logger::debug("Sent preface to client ID: " + std::to_string(id));
    }
    
    return bytesSent == static_cast<ssize_t>(preface.size());
}

bool Client::sendUpgradeHeader() {
    std::string upgradeHeader = "HTTP/1.1 101 Switching Protocols\r\n"
                                "Upgrade: h2c\r\n"
                                "Connection: Upgrade\r\n"
                                "\r\n";
    ssize_t bytesSent = send(clientFD.fd, upgradeHeader.c_str(), upgradeHeader.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
    
    if (bytesSent < 0) {
        errorCode = errno;
        Logger::error("Error sending upgrade header to client: " + std::to_string(errorCode));
        return false;
    } else if (bytesSent < static_cast<ssize_t>(upgradeHeader.size())) {
        Logger::warning("Partial upgrade header sent to client, expected: " + std::to_string(upgradeHeader.size()) +
                        ", sent: " + std::to_string(bytesSent));
    } else {
        Logger::debug("Sent upgrade header to client ID: " + std::to_string(id));
    }
    
    return bytesSent == static_cast<ssize_t>(upgradeHeader.size());
}

bool Client::applySettings() {
    http2::protocol::Settings settings;
    std::vector<uint8_t> encodedSettings;
    settings.encode(encodedSettings);

    http2::protocol::Frame settingsFrame(
        http2::protocol::SETTINGS_FRAME,
        http2::protocol::NO_FLAGS,
        0
    );
    settingsFrame.decode(encodedSettings);

    sendFrame(settingsFrame);

    // std::vector<uint8_t> recvBuffer(24);
    // int bytesRecv = recv(clientFD.fd, recvBuffer.data(), recvBuffer.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
    // if (bytesRecv < 0) {
    //     errorCode = errno;
    //     Logger::error("Error receiving settings ACK from client: " + std::to_string(errorCode));
    //     return false;
    // } else if (bytesRecv < static_cast<int>(recvBuffer.size())) {
    //     Logger::warning("Partial settings ACK received, expected: " + std::to_string(recvBuffer.size()) +
    //                     ", received: " + std::to_string(bytesRecv));
    // } else {
    //     Logger::debug("Received settings ACK from client ID: " + std::to_string(id));
    // }

    return true;
}

void Client::doRequest(epoll_event& event) {
    recvBuffer.resize(BUFFER_SIZE);
    ssize_t bytesRead = recv(clientFD.fd, recvBuffer.data(), recvBuffer.size(), MSG_DONTWAIT | MSG_NOSIGNAL);

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
    Logger::debug("Data: ");
    std::string bytes = "";
    for (ssize_t i = 0; i < bytesRead; ++i) {
        bytes += std::to_string(recvBuffer[i]) + " ";
    }
    Logger::debug(bytes);

    std::vector<http2::protocol::Frame> frames = http2::protocol::Frame::toFrames(
        recvBuffer.data(),
        recvBuffer.data() + bytesRead);

    Logger::info("Received " + std::to_string(frames.size()) + " frames from client ID: " + std::to_string(id));

    for(const auto& frame: frames) {
        if(handleFrame(frame)) {
            Logger::info("Handled frame of type: " + std::to_string(frame.type()) + 
                         " for client ID: " + std::to_string(id));
        } else {
            Logger::error("Error handling frame of type: " + std::to_string(frame.type()) + 
                          " for client ID: " + std::to_string(id));
        }
    }
}


bool Client::handleDataFrame(const http2::protocol::Frame& frame) {
    if(streams.find(frame.stream_id()) == streams.end()) {
        Logger::error("Data frame received for unknown stream ID: " + std::to_string(frame.stream_id()));
        return false;
    }

    Stream* stream = streams[frame.stream_id()];
    if(stream->state != StreamState::OPEN) {
        Logger::error("Data frame received for stream ID: " + std::to_string(frame.stream_id()) +
                      " in open state");
        return false;
    }

    stream->data.insert(stream->data.end(), frame.payload().begin(), frame.payload().end());
    stream->endStream = frame.has_flag(http2::protocol::END_STREAM);
    stream->endHeader = frame.has_flag(http2::protocol::END_HEADERS);
    return true;
}

bool Client::handleHeadersFrame(const http2::protocol::Frame& frame) {
    int streamId = frame.stream_id();
    bool endHeaders = frame.has_flag(http2::protocol::END_HEADERS);
    bool endStream = frame.has_flag(http2::protocol::END_STREAM);
    bool ok = true;

    if(streams.find(streamId) == streams.end()) {
        streams[streamId] = new Stream(streamId);
    }

    streams[streamId]->state = StreamState::OPEN;
    streams[streamId]->endHeader = endHeaders;
    streams[streamId]->endStream = endStream;
    streams[streamId]->headerFragments.insert(
        streams[streamId]->headerFragments.end(),
        frame.payload().begin(),
        frame.payload().end()
    );

    Stream* strm = streams[streamId];

    if(endHeaders) {
        ok |= processEndHeader(strm);
    }

    if(endStream) {
        strm->state = StreamState::CLOSED;
    }

    return ok;
}

bool Client::processEndHeader(Stream* strm) {
    try {
        http2::protocol::hpack::Decoder decoder;
        decoder.decode(strm->headerFragments, strm->headers);
        strm->state = StreamState::HALF_CLOSED_REMOTE;
        strm->endHeader = true;
        strm->headerFragments.clear();

        for (const auto& header : strm->headers) {
            Logger::debug("Header received: " + header.name + ": " + header.value);
        }
        return true;
    } catch (const std::exception& e) {
        Logger::error("Error processing end header: " + std::string(e.what()));
        return false;
    }
}

bool Client::handlePingFrame(const http2::protocol::Frame &frame) {return true;}
bool Client::handleGoAwayFrame(const http2::protocol::Frame &frame) {return true;}
bool Client::handleWindowUpdateFrame(const http2::protocol::Frame &frame) {return true;}
bool Client::handleContinuationFrame(const http2::protocol::Frame & frame) {return true;}
bool Client::handlePriorityFrame(const http2::protocol::Frame &frame) {return true;}
bool Client::handleResetFrame(const http2::protocol::Frame & frame) {return true;}
bool Client::handleSettingsFrame(const http2::protocol::Frame &frame) {return true;}
bool Client::handlePushPromiseFrame(const http2::protocol::Frame &frame) {return true;}