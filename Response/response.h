#include "http2/headers/headers.h"
#include "http2/protocol/hpack/hpack.h"
#include "http2/protocol/frame.h"
#include "vector"

#define MAX_BUFFER_SIZE 32768
#define MAX_FRAME_SIZE 16384-15 // 15 bytes as a quick hack for the preceding 15 bytes

class Response {
private:
    std::vector<http2::protocol::Frame> frames;
    http2::protocol::hpack::Encoder hpackEncoder;
    http2::protocol::hpack::Decoder hpackDecoder;
    std::vector<uint8_t> buffer;
public:
    Response() = default;

    void addFrame(const http2::protocol::Frame& frame) {
        frames.push_back(frame);
    }

    const std::vector<http2::protocol::Frame>& getFrames() const {
        return frames;
    }

    std::vector<http2::protocol::Frame> splitFrame(http2::protocol::Frame const& frame);
    void processFrames();
    
    std::vector<std::vector<uint8_t>> encodeFrames(); // Can use a generator for this but we are storing payload anyways

    
};