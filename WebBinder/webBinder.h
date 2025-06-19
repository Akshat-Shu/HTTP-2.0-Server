#include <filesystem>
#include <vector>
#include <map>
#include <fstream>
#include "Utils/Logger/logger.h"
#include "Response/responseData.h"
#include "http2/headers/statusCode.h"

#pragma once

class WebBinder {
private:
    std::map<std::string, std::string> dirBindings;
    std::map<std::string, std::string> fileBindings;

    static const ResponseData empty; 
public:
    WebBinder() = default;

    bool bindDirectory(const std::string& dir, const std::string& url);

    void bindFile(const std::string& file, const std::string& url);

    ResponseData getContent(const std::string& url); 
    
    ResponseData getFileContent(const std::string& file);

    ResponseData getDirectoryContent(const std::string& dir, std::string boundUrl); 

    ResponseData getErrorPage(int errorCode = 404);
};