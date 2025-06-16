#include "webBinder.h"

namespace fs = std::filesystem;

const ResponseData WebBinder::empty = ResponseData("text/plain", std::vector<u_int8_t>{});

bool WebBinder::bindDirectory(const std::string& dir, const std::string& url) {
    if (!fs::is_directory(dir)) {
        Logger::error("Failed to bind directory: " + dir + " - Not a valid directory");
        return false;
    }
    if (dirBindings.find(url) != dirBindings.end()) {
        Logger::warning("Directory already bound to URL: " + url + ", replacing with new directory: " + dir);
    }
    if(fileBindings.find(url) != fileBindings.end()) {
        Logger::warning("File already bound to URL: " + url + ", removing existing binding");
        fileBindings.erase(url);
    }
    dirBindings[url] = dir;
    return true;
}

void WebBinder::bindFile(const std::string& file, const std::string& url) {
    if (!fs::is_regular_file(file)) {
        Logger::error("Failed to bind file: " + file + " - Not a valid file");
        return;
    }
    if (fileBindings.find(url) != fileBindings.end()) {
        Logger::warning("File already bound to URL: " + url + ", replacing with new file: " + file);
    }
    if(dirBindings.find(url) != dirBindings.end()) {
        Logger::warning("Directory already bound to URL: " + url + ", removing existing binding");
        dirBindings.erase(url);
    }
    fileBindings[url] = file;
}

ResponseData WebBinder::getFileContent(const std::string& file) {
    if (!std::filesystem::is_regular_file(file)) {
        Logger::error("Failed to get content: " + file + " - Not a valid file");
        return empty;
    }

    std::ifstream inFile(file);
    if (!inFile) {
        Logger::error("Failed to open file: " + file);
        return empty;
    }
    
    return ResponseData::fromPath(file);
}

ResponseData WebBinder::getDirectoryContent(const std::string& dir, std::string boundUrl) {
    if (!std::filesystem::is_directory(dir)) {
        Logger::error("Failed to get directory content: " + dir + " - Not a valid directory");
        return empty;
    }

    if(boundUrl.back() != '/') boundUrl += '/';

    std::string document = "<html><body><h1>Index for " + dir + "</h1><ul>";
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_directory()) {
            document += "<li><img src=\"/html/folder.png\" alt=\"Folder Icon\" style=\"width:16px;height:16px;\"> ";
            document += "<a href=\"" + boundUrl + entry.path().filename().string() + "/\">" + entry.path().filename().string() + "/</a></li>";
        } else if (entry.is_regular_file()) {
            document += "<li><img src=\"/html/file.png\" alt=\"File Icon\" style=\"width:16px;height:16px;\"> ";
            document += "<a href=\"" + boundUrl + entry.path().filename().string() + "\">" + entry.path().filename().string() + "</a></li>";
        }
    }

    document += "</ul></body></html>";
    return ResponseData(
        "text/html",
        std::vector<u_int8_t>(document.begin(), document.end())
    );
}

ResponseData WebBinder::getContent(const std::string& url) {
    if(dirBindings.find(url) != dirBindings.end()) {
        return getDirectoryContent(dirBindings[url], url);
    } else if(fileBindings.find(url) != fileBindings.end()) {
        return getFileContent(fileBindings[url]);
    } else {
        bool found = false; std::string newUrl = "";
        for (const auto& [dirUrl, dirPath] : dirBindings) {
            if (url.find(dirUrl) == 0) {
                found = true;
                newUrl = dirPath + url.substr(dirUrl.length());
                break;
            }
        }

        if(found) {
            if(fs::is_directory(newUrl)) {
                return getDirectoryContent(newUrl, url);
            } else if(fs::is_regular_file(newUrl)) {
                return getFileContent(newUrl);
            } else {
                Logger::error("Path found but not a valid file or directory: " + newUrl);
                return empty;
            }
        } else 
            Logger::error("No binding found for URL: " + url);
        return empty;
    }
}