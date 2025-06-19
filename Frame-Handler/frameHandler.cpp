#include "frameHandler.h"
#include "Client/client.h"


bool FrameHandler::handleDataFrame(Client* client, const http2::protocol::Frame& frame) {
    if(client->streams.find(frame.stream_id()) == client->streams.end()) {
        Logger::error("Data frame received for unknown stream ID: " + std::to_string(frame.stream_id()));
        return false;
    }

    Stream* stream = client->streams[frame.stream_id()];
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

bool FrameHandler::handleHeadersFrame(Client* client, const http2::protocol::Frame& frame) {
    int streamId = frame.stream_id();
    bool endHeaders = frame.has_flag(http2::protocol::END_HEADERS);
    bool endStream = frame.has_flag(http2::protocol::END_STREAM);
    bool ok = true;

    if(client->streams.find(streamId) == client->streams.end()) {
        client->streams[streamId] = new Stream(streamId);
    }

    client->streams[streamId]->state = StreamState::OPEN;
    client->streams[streamId]->endHeader = endHeaders;
    client->streams[streamId]->endStream = endStream;
    client->streams[streamId]->headerFragments.insert(
        client->streams[streamId]->headerFragments.end(),
        frame.payload().begin(),
        frame.payload().end()
    );

    Stream* strm = client->streams[streamId];

    if(endHeaders) {
        ok |= processEndHeader(client, strm);
    }

    if(endStream) {
        strm->state = StreamState::CLOSED;
    }

    return ok;
}

bool FrameHandler::processEndHeader(Client* client, Stream* strm) {
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

            respondGet(client, strm);
        }
        

        return true;
    } catch (const std::exception& e) {
        Logger::error("Error processing end header: " + std::string(e.what()));
        return false;
    }
}

bool FrameHandler::handleWindowUpdateFrame(Client* client, const http2::protocol::Frame &frame) {return true;}
bool FrameHandler::handleResetFrame(Client* client, const http2::protocol::Frame & frame) {return true;}
bool FrameHandler::handlePushPromiseFrame(Client* client, const http2::protocol::Frame &frame) {return true;}

bool FrameHandler::handleSettingsFrame(Client* client, const http2::protocol::Frame &frame) {
    if (frame.stream_id() != 0) {
        Logger::error("Settings frame received with non-zero stream ID: " + std::to_string(frame.stream_id()));
        return false;
    }

    if (!frame.has_flag(http2::protocol::ACK)) {
        Logger::info("Received settings frame, applying settings");
        return client->ackSettings(frame);
    } else {
        Logger::info("Received settings ACK from client ID: " + std::to_string(client->id));
    }

    return true;
}

bool FrameHandler::handlePingFrame(Client* client, const http2::protocol::Frame &frame) {
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

    if (!client->sendFrame(pingAck)) {
        Logger::error("Failed to send PING ACK for client ID: " + std::to_string(client->id));
        return false;
    }

    return true;
}

bool FrameHandler::respondGet(Client* client, Stream* strm) {
    auto [foundPath, path] = strm->headers.first(":path");
    if(!foundPath) {
        Logger::error("No :path header found in headers for stream ID: " + std::to_string(strm->id));
        return false;
    }
    ResponseData content = client->binder->getContent(path);
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
        if(!client->sendData(frame, strm->weight)) {
            Logger::error("Failed to send response frame for stream ID: " + std::to_string(strm->id));
            return false;
        }
    }

    return true;
}

bool FrameHandler::handleGoAwayFrame(Client* client, const http2::protocol::Frame &frame) {
    Logger::info("Received GOAWAY frame from client ID: " + std::to_string(client->id));
    client->streams.clear();
    client->errorCode = http2::protocol::NO_ERROR;
    client->clientFD.setState(FdState::FD_CLOSED);
    return true;
}

bool FrameHandler::handleContinuationFrame(Client* client, const http2::protocol::Frame & frame) {
    int sid = frame.stream_id();

    if(client->streams.find(sid) == client->streams.end()) {
        Logger::error("Continuation frame received for unknown stream ID: " + std::to_string(sid));
        return false;
    }

    Stream* stream = client->streams[sid];
    if(stream->state != StreamState::OPEN && stream->state != StreamState::HALF_CLOSED_REMOTE) {
        Logger::error("Continuation frame received for stream ID: " + std::to_string(sid) +
                      " in state: " + std::to_string(stream->state));
        return false;
    }

    stream->headerFragments.insert(
        stream->headerFragments.end(),
        frame.payload().begin(),
        frame.payload().end()
    );
    stream->endHeader = frame.has_flag(http2::protocol::END_HEADERS);
    stream->endStream = frame.has_flag(http2::protocol::END_STREAM);

    if(stream->endHeader) {
        return processEndHeader(client, stream);
    }

    return true;
}

bool FrameHandler::handlePriorityFrame(Client* client, const http2::protocol::Frame &frame) {
    int streamId = frame.stream_id();
    if(client->streams.find(streamId) == client->streams.end()) {
        Logger::error("Priority frame received for unknown stream ID: " + std::to_string(streamId));
        return false;
    }

    Stream* stream = client->streams[streamId];
    if(!frame.isPriority()) {
        Logger::error("Priority frame received without priority flag for stream ID: " + std::to_string(streamId));
        return false;
    }

    stream->weight = frame.weight();
    stream->dependency = frame.streamDependency();
    stream->exclusive = frame.isExclusive();

    Logger::debug("Priority frame processed for stream ID: " + std::to_string(streamId) +
                  ", weight: " + std::to_string(stream->weight) +
                  ", dependency: " + std::to_string(stream->dependency) +
                  ", exclusive: " + (stream->exclusive ? "true" : "false"));

    return true;
}

bool FrameHandler::handleFrame(Client* client, const http2::protocol::Frame &frame) {
        Logger::debug("Handling frame of type: " + std::to_string(frame.type()) + 
                      " for client ID: " + std::to_string(client->id));
        switch (frame.type()) {
            case http2::protocol::DATA_FRAME:
                return handleDataFrame(client, frame);
            case http2::protocol::HEADERS_FRAME:
                return handleHeadersFrame(client, frame);
            case http2::protocol::PRIORITY_FRAME:
                return handlePriorityFrame(client, frame);
            case http2::protocol::RST_STREAM_FRAME:
                return handleResetFrame(client, frame);
            case http2::protocol::SETTINGS_FRAME:
                return handleSettingsFrame(client, frame);
            case http2::protocol::PUSH_PROMISE_FRAME:
                return handlePushPromiseFrame(client, frame);
            case http2::protocol::PING_FRAME:
                return handlePingFrame(client, frame);
            case http2::protocol::GOAWAY_FRAME:
                return handleGoAwayFrame(client, frame);
            case http2::protocol::WINDOW_UPDATE_FRAME:
                return handleWindowUpdateFrame(client, frame);
            case http2::protocol::CONTINUATION_FRAME:
                return handleContinuationFrame(client, frame);
            default:
                Logger::error("Unknown frame type: " + std::to_string(frame.type()));
                return false;
        }
    }