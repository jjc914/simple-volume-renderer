#include "volume.hpp"

#include "file.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

glm::vec3 Volume::Extent() const {
    glm::vec3 e = glm::vec3(float(width), float(height), float(depth)) * stretch;
    return e / std::max({e.x, e.y, e.z});
}

bool LoadVolume(const std::string& rawPath, Volume* out) {
    // --- Metadata ----------------------------------------------------------
    std::string kmd = ReadFile(rawPath + ".kmd");
    if (kmd.empty()) return false;   // ReadFile already logged

    Volume v;
    int type = -1;
    std::istringstream lines(kmd);
    std::string line;
    while (std::getline(lines, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::istringstream values(line.substr(colon + 1));
        if (key == "size")         values >> v.width >> v.height >> v.depth;
        else if (key == "type")    values >> type;
        else if (key == "stretch") values >> v.stretch.x >> v.stretch.y >> v.stretch.z;
    }

    if (v.width <= 0 || v.height <= 0 || v.depth <= 0) {
        std::cerr << "LoadVolume: missing or invalid size in " << rawPath << ".kmd" << std::endl;
        return false;
    }
    if (type != 2) {
        std::cerr << "LoadVolume: unsupported type " << type << " in " << rawPath
                  << ".kmd (only 2 = float32)" << std::endl;
        return false;
    }
    if (v.stretch.x <= 0 || v.stretch.y <= 0 || v.stretch.z <= 0) v.stretch = glm::vec3(1.0f);

    // --- Voxels ------------------------------------------------------------
    std::filesystem::path path = std::filesystem::current_path() / rawPath;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file: " << path << std::endl;
        return false;
    }

    size_t count = size_t(v.width) * v.height * v.depth;
    v.voxels.resize(count);
    file.read(reinterpret_cast<char*>(v.voxels.data()), std::streamsize(count * sizeof(float)));
    if (size_t(file.gcount()) != count * sizeof(float)) {
        std::cerr << "LoadVolume: " << rawPath << " is smaller than the "
                  << v.width << "x" << v.height << "x" << v.depth << " float32 grid it declares" << std::endl;
        return false;
    }

    // Rescale to [0, 1], which is what the transfer function is indexed by.
    auto [lo, hi] = std::minmax_element(v.voxels.begin(), v.voxels.end());
    v.minValue = *lo;
    v.maxValue = *hi;
    float range = v.maxValue - v.minValue;
    float inv = range > 0.0f ? 1.0f / range : 0.0f;
    for (float& x : v.voxels) x = (x - v.minValue) * inv;

    *out = std::move(v);
    return true;
}
