#ifndef VOLUME_HPP
#define VOLUME_HPP

#include <glm/glm.hpp>

#include <string>
#include <vector>

// A dense scalar grid loaded from a headerless little-endian float32 ".raw"
// file plus a ".kmd" sidecar describing it, e.g. for "carp.raw":
//
//   carp.raw.kmd:   size: 512 256 256     (voxels in x y z)
//                   type: 2               (float32; the only type supported)
//                   stretch: 2 1 1        (relative voxel spacing, optional)
struct Volume {
    int width = 0, height = 0, depth = 0;
    glm::vec3 stretch{1.0f};
    float minValue = 0.0f, maxValue = 0.0f;   // range of the raw data
    std::vector<float> voxels;                // x fastest; rescaled to [0, 1]

    // Physical size, normalized so the longest side is 1.
    glm::vec3 Extent() const;
};

// Loads `rawPath` (relative to the working directory) and its ".kmd" file, and
// rescales the data to [0, 1]. Returns false and logs to stderr on failure.
bool LoadVolume(const std::string& rawPath, Volume* out);

#endif
