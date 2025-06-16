#include "http2/protocol/frame.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <Utils/Logger/logger.h>

namespace http2 {
namespace protocol {

Frame::operator std::string() const { // for printing
  std::ostringstream out;
  out << '[' << std::hex << std::setfill('0') << std::setw(2) << uint16_t(type_)
      << ',' << std::setw(2) << uint16_t(flags_)  //
      << ',' << std::setw(8) << sid_              //
      << '|';
  const uint8_t* ptr = payload_.data();
  std::size_t len = payload_.size();
  while (len > 0) {
    out << std::setw(2) << uint16_t(*ptr) << ((len > 1) ? "," : "");
    ++ptr;
    --len;
  }
  out << ']';
  return out.str();
}

std::vector<uint8_t> Frame::encode(bool debug) const { // getFrame (encode it)
  // std::cerr << "DEBUG: " << *this << std::endl;
  uint32_t s = payload_.size();
  if (s > 0x00ffffffUL) abort();
  std::vector<uint8_t> frame;
  frame.reserve(9 + s);
  frame.insert(frame.end(), {
                             uint8_t(s >> 16),     // Size (hi byte)
                             uint8_t(s >> 8),      // Size (mid byte)
                             uint8_t(s),           // Size (lo byte)
                             type_,                // Type byte
                             flags_,               // Flags byte
                             uint8_t(sid_ >> 24),  // Stream ID (hi byte)
                             uint8_t(sid_ >> 16),  // Stream ID (mid-hi byte)
                             uint8_t(sid_ >> 8),   // Stream ID (mid-lo byte)
                             uint8_t(sid_),        // Stream ID (lo byte)
                            });
  frame.insert(frame.end(), payload_.begin(), payload_.end());
  return frame;
}

bool Frame::decode(const uint8_t* p, const uint8_t* q) {
  if(q - p < 9) return false;

  uint32_t size = (p[0] << 16) | (p[1] << 8) | p[2];
  if (size > 0x00ffffffUL || q - p < 9 + size) return false;

  type_ = p[3];
  flags_ = p[4];

  sid_ = ((p[5] & 0x7F) << 24) | (p[6] << 16) | (p[7] << 8) | p[8];
  p = p + 9;
  size_t payload_size = size;
  size_t headerStart = 0;

  // I also need to handle flags like PADDED, PRIORITY which modify the frame here.
  if((flags_ & PADDED) && (type_ == HEADERS_FRAME || type_ == PUSH_PROMISE_FRAME || type_ == DATA_FRAME)) {
    uint8_t padLength = *p;
    if(padLength > payload_size - 1) return false;
    headerStart++;
    payload_size -= (padLength + 1);
    p++;
  }

  if((flags_ & PRIORITY) && (type_ == HEADERS_FRAME || type_ == PUSH_PROMISE_FRAME)) {
    if(payload_size < 5 + headerStart) return false;
     
    hasPriority_ = true;
    exclusive_ = (p[0] & 0x80) != 0;
    streamDependency_ = ((p[0] & 0x7F) << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    weight_ = p[4] + 1;
    if (weight_ < 1 || weight_ > 256) return false;

    headerStart += 5;
    payload_size -= 5;
    p += 5;
  }

  if (payload_size < headerStart) return false;
  payload_.assign(p, p + payload_size);

  return true;
}

std::vector<Frame> Frame::toFrames(const uint8_t* begin, const uint8_t* end) {
  std::vector<Frame> frames;
  uint8_t* curr = (const_cast<uint8_t*>(begin));
  while (curr < end) {
    uint32_t size = (curr[0] << 16) | (curr[1] << 8) | curr[2];
    Frame frame;
    if (!frame.decode(curr, end)) break;
    frames.push_back(std::move(frame));
    curr += 9 + size;
  }
  return frames;
}

}  // namespace protocol
}  // namespace http2
