#include "env_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>

// Trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// Trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// Trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

std::map<std::string, std::string> load_env(const std::string& filename) {
    std::map<std::string, std::string> env_map;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return env_map;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        trim(line);
        if (line.empty()) continue;

        size_t delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);
            trim(key);
            trim(value);
            
            // Handle quotes if present
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            
            env_map[key] = value;
        }
    }
    return env_map;
}

std::string get_env_value(const std::map<std::string, std::string>& env, const std::string& key, const std::string& default_value) {
    auto it = env.find(key);
    if (it != env.end()) {
        return it->second;
    }
    return default_value;
}
