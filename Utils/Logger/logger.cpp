#include "logger.h"


void Logger::log(const std::string& level, const std::string& message, LogColor color) {
    if (logColors.find(level) != logColors.end()) {
        color = logColors[level];
    } else {
        color = RESET;
        std::cerr << "Unknown log level: " << level << std::endl;
        return;
    }
    std::cout << getColorCode(color) << "[" << level << "] " << message << getColorCode(RESET) << std::endl;
}