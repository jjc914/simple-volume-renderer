#ifndef OBJ_HPP
#define OBJ_HPP

#include <tiny_obj_loader.h>

#include <unordered_map>
#include <functional>
#include <iostream>

struct Mesh {
    bool hasNormals = true;
    bool hasTexCoords = true;
    std::vector<uint32_t> indices;
    std::vector<float> vertexAttributes;
};

struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];

    bool operator==(const Vertex& other) const {
        const float epsilon = 1e-6f;

        for (int i = 0; i < 3; ++i) {
            if (std::fabs(position[i] - other.position[i]) > epsilon) {
                return false;
            }
            if (std::fabs(normal[i] - other.normal[i]) > epsilon) {
                return false;
            }
        }

        for (int i = 0; i < 2; ++i) {
            if (std::fabs(uv[i] - other.uv[i]) > epsilon) {
                return false;
            }
        }

        return true;
    }
};

namespace std {
    template <>
    struct hash<Vertex> {
        size_t operator()(const Vertex& vertex) const {
            size_t hash = 0;
            for (const float& v : vertex.position) {
                hash ^= std::hash<float>()(v) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            for (const float& v : vertex.normal) {
                hash ^= std::hash<float>()(v) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            for (const float& v : vertex.uv) {
                hash ^= std::hash<float>()(v) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };
}

// assumes one shape
Mesh Load(const char* rpath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, rpath);
    if (!success) {
        std::cerr << "Failed to load OBJ file: " << err << std::endl;
        return Mesh{};
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (int i = 0; i < shapes.size(); ++i) {
        tinyobj::shape_t& shape = shapes[i];
        tinyobj::mesh_t& mesh = shape.mesh;
        for (int j = 0; j < mesh.indices.size(); ++j) {
            tinyobj::index_t index = mesh.indices[j];
            vertices.emplace_back();
            for (int k = 0; k < 3; ++k) {
                vertices.back().position[k] = attrib.vertices[index.vertex_index * 3 + k];
            }
            for (int k = 0; k < 3; ++k) {
                vertices.back().normal[k] = attrib.normals[index.normal_index * 3 + k];
            }
            for (int k = 0; k < 2; ++k) {
                vertices.back().uv[k] = attrib.texcoords[index.texcoord_index * 2 + k];
            }
        }
    }

    std::vector<float> vertexAttributes;
    for (int i = 0; i < vertices.size(); ++i) {
        vertexAttributes.emplace_back(vertices[i].position[0]);
        vertexAttributes.emplace_back(vertices[i].position[1]);
        vertexAttributes.emplace_back(vertices[i].position[2]);
        vertexAttributes.emplace_back(vertices[i].normal[0]);
        vertexAttributes.emplace_back(vertices[i].normal[1]);
        vertexAttributes.emplace_back(vertices[i].normal[2]);
        vertexAttributes.emplace_back(vertices[i].uv[0]);
        vertexAttributes.emplace_back(vertices[i].uv[1]);
        indices.emplace_back(i);
    }

    return Mesh{
        true, true,
        indices,
        vertexAttributes
    };
}

#endif