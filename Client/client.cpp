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
}

bool Client::isTimedOut(const int timeout) const {
    return difftime(time(nullptr), start) > timeout;
}

bool Client::sendData(const std::vector<uint8_t>& data) {
    Logger::debug("Data: " + toHex(data.data(), data.size()) +
                  ", size: " + std::to_string(data.size()) +
                  ", for client ID: " + std::to_string(id));

    auto bytesSent = threadPool->enqueue([this, data]() {
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

    return bytesSent.get() >= 0;
}

bool Client::sendFrame(const http2::protocol::Frame& frame) {
    std::vector<uint8_t> encodedFrame = frame.encode();

    return sendData(encodedFrame);
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
    ssize_t bytesRead = SSL_read(ssl, recvBuffer.data(), recvBuffer.size());

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
    Logger::debug("Data: " + toHex(recvBuffer.data(), bytesRead));

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
        decoder.decode_lowmem(strm->headerFragments.data(), 
                       strm->headerFragments.data() + strm->headerFragments.size(),
                       [&strm](http2::headers::Header header) {
                           strm->headers.add(std::move(header));
                       });
        strm->state = StreamState::HALF_CLOSED_REMOTE;
        strm->endHeader = true;
        strm->headerFragments.clear();

        for (const auto& header : strm->headers.all()) {
            Logger::debug("Header received: " + header.name + ": " + header.value);
        }

        auto [found, method] = strm->headers.first(":method");
        if(!found) {
            Logger::error("No :method header found in headers for stream ID: " + std::to_string(strm->id));
            return false;
        }

        Logger::debug("Method for stream ID " + std::to_string(strm->id) + ": " + method);
        if(method == "GET") {
            auto [foundPath, path] = strm->headers.first(":path");
            if(!foundPath) {
                Logger::error("No :path header found in headers for stream ID: " + std::to_string(strm->id));
                return false;
            }

            respondGet(strm);
        }
        

        return true;
    } catch (const std::exception& e) {
        Logger::error("Error processing end header: " + std::string(e.what()));
        return false;
    }
}

bool Client::handleGoAwayFrame(const http2::protocol::Frame &frame) {return true;}
bool Client::handleWindowUpdateFrame(const http2::protocol::Frame &frame) {return true;}
bool Client::handleContinuationFrame(const http2::protocol::Frame & frame) {return true;}
bool Client::handlePriorityFrame(const http2::protocol::Frame &frame) {return true;}
bool Client::handleResetFrame(const http2::protocol::Frame & frame) {return true;}
bool Client::handlePushPromiseFrame(const http2::protocol::Frame &frame) {return true;}

bool Client::handleSettingsFrame(const http2::protocol::Frame &frame) {
    if (frame.stream_id() != 0) {
        Logger::error("Settings frame received with non-zero stream ID: " + std::to_string(frame.stream_id()));
        return false;
    }

    if (!frame.has_flag(http2::protocol::ACK)) {
        Logger::info("Received settings frame, applying settings");
        return ackSettings(frame);
    } else {
        Logger::info("Received settings ACK from client ID: " + std::to_string(id));
    }

    return true;
}

bool Client::handlePingFrame(const http2::protocol::Frame &frame) {
    if (frame.stream_id() != 0) {
        Logger::error("Ping frame received with non-zero stream ID: " + std::to_string(frame.stream_id()));
        return false;
    }

    http2::protocol::Frame pingAck(
        http2::protocol::PING_FRAME,
        http2::protocol::ACK,
        0
    );

    pingAck.mutable_payload() = frame.payload();

    if (!sendFrame(pingAck)) {
        Logger::error("Failed to send PING ACK for client ID: " + std::to_string(id));
        return false;
    }

    return true;
}

bool Client::respondGet(Stream* strm) {
    auto [foundPath, path] = strm->headers.first(":path");
    if(!foundPath) {
        Logger::error("No :path header found in headers for stream ID: " + std::to_string(strm->id));
        return false;
    }
    ResponseData content = binder->getContent(path);
    if(content.data.empty()) {
        Logger::error("No content found for path: " + path);
        return false;
    }

    // Logger::debug("Content for path " + path + ": " + htmlContent);

    http2::protocol::Frame headerFrame(
        http2::protocol::HEADERS_FRAME,
        http2::protocol::END_HEADERS,
        strm->id
    );

    http2::headers::Headers responseHeaders;
    responseHeaders.add(":status", "200");
    responseHeaders.add("cache-control", "private");
    responseHeaders.add("content-type", content.mimeType);
    responseHeaders.add("content-length", std::to_string(content.data.size()));
    responseHeaders.add("location", path);
    responseHeaders.add("server", "HTTP2Server/1.0");
    std::vector<uint8_t> encodedHeaders;
    http2::protocol::hpack::Encoder hpackEncoder;
    hpackEncoder.encode_all(responseHeaders.all(), encodedHeaders);

    headerFrame.mutable_payload().insert(
        headerFrame.mutable_payload().end(),
        encodedHeaders.begin(),
        encodedHeaders.end()
    );

    http2::protocol::Frame dataFrame(
        http2::protocol::DATA_FRAME,
        http2::protocol::END_STREAM,
        strm->id
    );

    dataFrame.mutable_payload().insert(
        dataFrame.mutable_payload().end(),
        content.data.begin(),
        content.data.end()
    );

    Response resp;
    resp.addFrame(headerFrame);
    resp.addFrame(dataFrame);
    resp.processFrames();
    
    for(const auto& frame : resp.encodeFrames()) {
        if(!sendData(frame)) {
            Logger::error("Failed to send response frame for stream ID: " + std::to_string(strm->id));
            return false;
        }
    }

    return true;
}