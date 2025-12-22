#ifndef ENV_LOADER_H
#define ENV_LOADER_H

#include <string>
#include <map>

// Loads .env file from the current directory into a map
std::map<std::string, std::string> load_env(const std::string& filename);

// Helper to get value or default
std::string get_env_value(const std::map<std::string, std::string>& env, const std::string& key, const std::string& default_value = "");

#endif // ENV_LOADER_H
