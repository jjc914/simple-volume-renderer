#include "file.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

std::string ReadFile(const std::string& rpath) {
    std::filesystem::path path = std::filesystem::current_path() / rpath;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file: " << path << std::endl;
        return "";
    }

    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}
