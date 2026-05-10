#include "ply.h"
#include <omp.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

PLYModel loadPLY(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open file");

    PLYModel model;
    std::vector<std::string> properties;
    int vertexCount = 0;
    std::string line;

    while (std::getline(file, line)) {
        if (line.rfind("element vertex", 0) == 0) {
            vertexCount = std::stoi(line.substr(15));
        } else if (line.rfind("property float", 0) == 0) {
            std::string name = line.substr(15);
            if (!name.empty() && name.back() == '\r') name.pop_back();
            properties.push_back(name);
        } else if (line == "end_header" || line == "end_header\r") {
            break;
        }
    }

    if (vertexCount <= 0 || vertexCount > 10000000)
        throw std::runtime_error("Suspicious vertex count");

    // Build offset map
    std::unordered_map<std::string, int> offsets;
    int off = 0;
    for (auto& p : properties) { offsets[p] = off; off += sizeof(float); }
    int stride = off;

    // Sanity check blob size
    size_t blobSize = (size_t)stride * vertexCount;
    if (blobSize > 1ULL * 1024 * 1024 * 1024)
        throw std::runtime_error("Blob too large");

    // Read blob
    std::vector<uint8_t> blob(blobSize);
    file.read(reinterpret_cast<char*>(blob.data()), blobSize);
    if (file.gcount() != (std::streamsize)blobSize)
        throw std::runtime_error("File truncated");
    printf("Read %d gaussians, unpacking...\n", vertexCount);

    auto getFloat = [&](int vertex, const std::string& name) -> float {
        auto it = offsets.find(name);
        if (it == offsets.end()) return 0.0f;
        float val;
        memcpy(&val, blob.data() + vertex * stride + it->second, sizeof(float));
        return val;
    };

    int off_rest[45];
    for (int i = 0; i < 45; i++) {
        auto it = offsets.find("f_rest_" + std::to_string(i));
        off_rest[i] = (it != offsets.end()) ? it->second : -1;
    }

    // Unpack directly into GaussianVertex
    model.gaussians.resize(vertexCount);

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < vertexCount; i++) {
        GaussianVertex& v = model.gaussians[i];
        const uint8_t* row = blob.data() + i * stride;
        // 0.5f + 0.2820948f
        // Read directly by offset instead of hash lookup
        v.x =  *(float*)(row + offsets["x"]);
        v.y = -*(float*)(row + offsets["y"]);
        v.z = -*(float*)(row + offsets["z"]);

        v.f_dc[0] = *(float*)(row + offsets["f_dc_0"]);
        v.f_dc[1] = *(float*)(row + offsets["f_dc_1"]);
        v.f_dc[2] = *(float*)(row + offsets["f_dc_2"]);

        for (int i = 0; i < 45; i++) {
            v.f_rest[i] = (off_rest[i] >= 0) ? *(float*)(row + off_rest[i]) : 0.0f;
        }

        v.opacity = 1.0f / (1.0f + expf(-*(float*)(row + offsets["opacity"])));

        v.scale[0] = expf(*(float*)(row + offsets["scale_0"]));
        v.scale[1] = expf(*(float*)(row + offsets["scale_1"]));
        v.scale[2] = expf(*(float*)(row + offsets["scale_2"]));

        v.rot[0] =  *(float*)(row + offsets["rot_0"]);
        v.rot[1] =  *(float*)(row + offsets["rot_1"]);
        v.rot[2] = -*(float*)(row + offsets["rot_2"]);
        v.rot[3] = -*(float*)(row + offsets["rot_3"]);
    }

    // Free blob before returning
    blob = std::vector<uint8_t>();

    printf("Done unpacking.\n");
    return model;
}
