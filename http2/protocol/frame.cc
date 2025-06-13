#include "http2/protocol/frame.h"

#include <iostream>
#include <iomanip>
#include <sstream>

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
  std::cerr << "DEBUG: " << *this << std::endl;
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

  sid_ = (p[5] << 24) | (p[6] << 16) | (p[7] << 8) | p[8];
  payload_.assign(p + 9, p + 9 + size);

  return true;
}

std::vector<Frame> toFrames(const uint8_t* begin, const uint8_t* end) {
  std::vector<Frame> frames;
  uint8_t* curr = (const_cast<uint8_t*>(begin));
  while (curr < end) {
    Frame frame;
    if (!frame.decode(curr, end)) break;
    frames.push_back(std::move(frame));
    curr += 9 + frame.payload().size();
  }
  return frames;
}

}  // namespace protocol
}  // namespace http2
