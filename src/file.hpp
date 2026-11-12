#ifndef FILE_HPP
#define FILE_HPP

#include <string>

// Reads a file (path relative to the working directory) into a string.
// Returns an empty string and logs to stderr on failure.
std::string ReadFile(const std::string& rpath);

#endif
