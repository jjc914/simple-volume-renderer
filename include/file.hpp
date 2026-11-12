#ifndef FILE_HPP
#define FILE_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

std::string ReadFile(const std::string& rpath) {
    std::filesystem::path path = std::filesystem::current_path() / rpath;

    std::ifstream shaderFile(path);
    if (!shaderFile.is_open()) {
        std::cerr << "Error: Could not open file: " << path << std::endl;
        return "";
    }

    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    return shaderStream.str();
}

#endif