#include "statusCode.h"

extern const std::map<int, std::string> http2::status::errorMessages = {
    {HTTP2_OK, "OK"},
    {HTTP2_NO_CONTENT, "No Content"},
    {HTTP2_BAD_REQUEST, "Bad Request"},
    {HTTP2_UNAUTHORIZED, "Unauthorized"},
    {HTTP2_FORBIDDEN, "Forbidden"},
    {HTTP2_NOT_FOUND, "Not Found"},
    {HTTP2_INTERNAL_SERVER_ERROR, "Internal Server Error"},
    {HTTP2_NOT_IMPLEMENTED, "Not Implemented"},
    {HTTP2_SERVICE_UNAVAILABLE, "Service Unavailable"}
};

extern std::string http2::status::getMessage(int code) {
    auto it = errorMessages.find(code);
    if (it != errorMessages.end()) {
        return it->second;
    }
    return "Unknown Status Code";
}