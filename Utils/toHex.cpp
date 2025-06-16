#include <sstream>
#include <vector>
#include <stdint.h>
#include <iomanip>

using namespace std;


inline string toHex(const uint8_t* data, size_t length, size_t maxLength = 128) {
    std::ostringstream oss; int i = 0;
    for (size_t j = 0; j < min((size_t)maxLength, length); ++j) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[j]);
        oss << " "; if(i++ % 16 == 15) oss << "\n";
    }
    if (length > maxLength) oss << "...";
    
    return oss.str();
}

inline string toHex(vector<uint8_t> const& data) {
    return toHex(data.data(), data.size());
}

inline string toHex(const uint8_t byte) {
    std::ostringstream oss;
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return oss.str();
}