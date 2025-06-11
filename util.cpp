#include <string>
#include <map>

class Util {
public:
    static std::string strip(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(start, end - start + 1);
    }

    static std::string toLower(const std::string& str) {
        std::string lowerStr = str;
        for (char& c : lowerStr) c = tolower(c);
        return lowerStr;
    }

    static std::string toUpper(const std::string& str) {
        std::string upperStr = str;
        for (char& c : upperStr) c = toupper(c);
        return upperStr;
    }

    static std::string percentDecode(const std::string& str) {
        std::string decoded;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '%' && i + 2 < str.length()) {
                std::string hex = str.substr(i + 1, 2);
                char decodedChar = static_cast<char>(std::stoi(hex, nullptr, 16));
                decoded += decodedChar;
                i += 2;
            } else if (str[i] == '+') {
                decoded += ' ';
            } else {
                decoded += str[i];
            }
        }
        return decoded;
    }

    static std::map<std::string, std::string> parseQueryString(const std::string& query) {
        std::map<std::string, std::string> params;
        size_t start = 0;
        while (start < query.length()) {
            size_t end = query.find('&', start);
            if (end == std::string::npos) end = query.length();
            std::string param = query.substr(start, end - start);
            size_t equalPos = param.find('=');
            if (equalPos != std::string::npos) {
                std::string key = percentDecode(param.substr(0, equalPos));
                std::string value = percentDecode(param.substr(equalPos + 1));
                key = strip(key);
                value = strip(value);
                params[key] = value;
            }
            start = end + 1;
        }
        return params;
    }
};