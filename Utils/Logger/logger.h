#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>

enum LogColor {
    RESET = 0,
    BLACK = 30,
    RED = 31,
    GREEN = 32,
    YELLOW = 33,
    BLUE = 34,
    MAGENTA = 35,
    CYAN = 36,
    WHITE = 37
};


class Logger {
    static void log(const std::string& level, const std::string& message, LogColor color= RESET); 
    
    const static std::map<std::string, LogColor> logColors; 

    static std::string getColorCode(LogColor color) {
        return "\033[" + std::to_string(color) + "m";
    }

public:
    static void debug(const std::string &message) {
        log("DEBUG", message, BLUE);
    }

    static void info(const std::string &message) {
        log("INFO", message, GREEN);
    }

    static void warning(const std::string &message) {
        log("WARNING", message, YELLOW);
    }

    static void error(const std::string &message) {
        log("ERROR", message, RED);
    }

    static void fatal(const std::string &message) {
        log("FATAL", message, MAGENTA);
        std::exit(EXIT_FAILURE);
    }
};