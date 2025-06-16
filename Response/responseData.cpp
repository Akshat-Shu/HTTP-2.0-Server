#include "responseData.h"


bool ResponseData::readFromFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);

    if (!file) {
        Logger::error("Failed to open file: " + filePath);
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) {
        Logger::error("Failed to read file: " + filePath);
        return false;
    }

    return true;
}

std::string ResponseData::getMimeType(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos == std::string::npos) return "application/octet-stream";

        std::string ext = path.substr(pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if(ext == "html" || ext == "htm") return "text/html";
        if(ext == "css") return "text/css";
        if(ext == "js") return "application/javascript";
        if(ext == "json") return "application/json";
        if(ext == "png") return "image/png";
        if(ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if(ext == "gif") return "image/gif";
        if(ext == "svg") return "image/svg+xml";
        if(ext == "txt") return "text/plain";
        if(ext == "xml") return "application/xml";
        if(ext == "pdf") return "application/pdf";

        return "application/octet-stream";
    }