#include <string>
#include <map>
#pragma once

enum Http2StatusCode {
    HTTP2_OK = 200,
    HTTP2_NO_CONTENT = 204,
    HTTP2_BAD_REQUEST = 400,
    HTTP2_UNAUTHORIZED = 401,
    HTTP2_FORBIDDEN = 403,
    HTTP2_NOT_FOUND = 404,
    HTTP2_INTERNAL_SERVER_ERROR = 500,
    HTTP2_NOT_IMPLEMENTED = 501,
    HTTP2_SERVICE_UNAVAILABLE = 503
};

namespace http2 {
namespace status {
extern const std::map<int, std::string> errorMessages;
extern std::string getMessage(int code);
}
}