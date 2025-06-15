#include "logger.h"


const std::map<std::string, LogColor> Logger::logColors = {
    {"DEBUG", BLUE},
    {"INFO", GREEN},
    {"WARNING", YELLOW},
    {"ERROR", RED},
    {"FATAL", MAGENTA}
};

void Logger::log(const std::string& level, const std::string& message, LogColor color) {
    if (logColors.find(level) != logColors.end()) {
        color = logColors.at(level);
    } else {
        color = RESET;
        std::cerr << "Unknown log level: " << level << std::endl;
        return;
    }
    std::cout << getColorCode(color) << "[" << level << "] " << message << getColorCode(RESET) << std::endl;
}