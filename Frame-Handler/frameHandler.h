#include "http2/protocol/frame.h"
#include "Utils/Logger/logger.h"
#include "Client/stream.h"
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
    static bool handleFrame(Client* client, const http2::protocol::Frame& frame); 

    static bool respondGet(Client* client, Stream* stream);
    static bool processEndHeader(Client* client, Stream* stream);
    static bool showErrorPage(Client* client, Stream* stream, int errorCode = 404);
};