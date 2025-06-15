#include "webBinder.h"


namespace fs = std::filesystem;

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

std::string WebBinder::getFileContent(const std::string& file) {
    if (!std::filesystem::is_regular_file(file)) {
        Logger::error("Failed to get content: " + file + " - Not a valid file");
        return "";
    }

    std::ifstream inFile(file);
    if (!inFile) {
        Logger::error("Failed to open file: " + file);
        return "";
    }
    
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    return buffer.str();
}

std::string WebBinder::getDirectoryContent(const std::string& dir) {
    if (!std::filesystem::is_directory(dir)) {
        Logger::error("Failed to get directory content: " + dir + " - Not a valid directory");
        return "";
    }

    std::string document = "<html><body><h1>Index for " + dir + "</h1><ul>";
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_directory()) {
            document += "<img src=\"/folder.png\" alt=\"Folder Icon\" style=\"width:16px;height:16px;\"> ";
            document += "<li><a href=\"" + entry.path().filename().string() + "/\">" + entry.path().filename().string() + "/</a></li>";
        } else if (entry.is_regular_file()) {
            document += "<img src=\"/file.png\" alt=\"File Icon\" style=\"width:16px;height:16px;\"> ";
            document += "<li><a href=\"" + entry.path().filename().string() + "\">" + entry.path().filename().string() + "</a></li>";
        }
    }

    document += "</ul></body></html>";
    return document;
}