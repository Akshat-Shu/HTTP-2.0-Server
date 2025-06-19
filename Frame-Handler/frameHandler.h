#include "http2/protocol/frame.h"
#include "Logger/logger.h"
#pragma once


class Client;

class FrameHandler {
public:
    static bool handleDataFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handleHeadersFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handlePriorityFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handleResetFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handleSettingsFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handlePushPromiseFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handlePingFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handleGoAwayFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handleWindowUpdateFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handleContinuationFrame(Client* client, const http2::protocol::Frame& frame);
    static bool handleFrame(Client* client, const http2::protocol::Frame& frame) {
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

    static bool respondGet(Client* client, Stream* stream);
    static bool processEndHeader(Client* client, Stream* stream);
};