#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include "Utils/Logger/logger.h"

class ResponseData {
public:
    std::string mimeType;
    std::vector<u_int8_t> data;

    ResponseData() = default;

    ResponseData(const std::string& mimeType, const std::vector<u_int8_t>& data)
        : mimeType(mimeType), data(data) {}

    static ResponseData fromPath(const std::string& path) {
        ResponseData resp;
        resp.mimeType = getMimeType(path);
        resp.readFromFile(path);
        
        return resp;
    }

    static std::string getMimeType(const std::string& path); 

    bool readFromFile(const std::string& filePath); 
};