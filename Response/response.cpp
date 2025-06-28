#include "response.h"

std::vector<std::vector<uint8_t>> Response::encodeFrames() {
    std::vector<std::vector<uint8_t>> encodedFrames(1);
    size_t currSize = 0, ctr = 0;
    for(auto& frame: frames) {
        if(currSize + frame.payload().size() + 9 > MAX_FRAME_SIZE) {
            encodedFrames.push_back(std::vector<uint8_t>());
            currSize = 0;
            ctr++;
        }
        std::vector<uint8_t> encodedFrame = frame.encode();
        encodedFrames[ctr].insert(encodedFrames[ctr].end(), encodedFrame.begin(), encodedFrame.end());
        currSize += encodedFrame.size();
    }

    return encodedFrames;
}


void Response::processFrames() {
    std::vector<http2::protocol::Frame> newFrames;
    for (const auto& frame : frames) {
        if(frame.payload().size() > MAX_FRAME_SIZE) {
            std::vector<http2::protocol::Frame> splitFrames = splitFrame(frame);
            newFrames.insert(newFrames.end(), splitFrames.begin(), splitFrames.end());
        } else {
            newFrames.push_back(frame);
        }
    }
    frames = std::move(newFrames);
}

std::vector<http2::protocol::Frame> Response::splitFrame(const http2::protocol::Frame& frame) {
    std::vector<http2::protocol::Frame> splitFrames;
    if (frame.size() <= MAX_FRAME_SIZE) {
        splitFrames.push_back(frame);
        return splitFrames;
    }

    uint8_t flags = frame.flags();
    bool endStream = frame.has_flag(http2::protocol::END_STREAM);

    if(endStream) flags ^= http2::protocol::END_STREAM;
    size_t offset = 0;
    while (offset < frame.payload().size()) {
        size_t chunkSize = std::min((size_t)MAX_FRAME_SIZE, frame.payload().size() - offset);
        http2::protocol::Frame newFrame(frame.type(), flags, frame.stream_id());
        newFrame.mutable_payload().insert(newFrame.mutable_payload().end(),
                                          frame.payload().begin() + offset,
                                          frame.payload().begin() + offset + chunkSize);
        splitFrames.push_back(newFrame);
        offset += chunkSize;
    }

    if(endStream) splitFrames.back().set_flags(splitFrames.back().flags() | http2::protocol::END_STREAM);
    return splitFrames;
}