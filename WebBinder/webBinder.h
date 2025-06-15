#include <filesystem>
#include <vector>
#include <map>
#include <fstream>
#include "Utils/Logger/logger.h"

#pragma once

class WebBinder {
private:
    std::map<std::string, std::string> dirBindings;
    std::map<std::string, std::string> fileBindings;
public:
    WebBinder() = default;

    bool bindDirectory(const std::string& dir, const std::string& url);

    void bindFile(const std::string& file, const std::string& url);

    std::string getContent(const std::string& url) {
        if(dirBindings.find(url) != dirBindings.end()) {
            return getDirectoryContent(dirBindings[url]);
        } else if(fileBindings.find(url) != fileBindings.end()) {
            return getFileContent(fileBindings[url]);
        } else {
            Logger::error("No binding found for URL: " + url);
            return "";
        }
    };
    
    std::string getFileContent(const std::string& file);

    std::string getDirectoryContent(const std::string& dir);
};