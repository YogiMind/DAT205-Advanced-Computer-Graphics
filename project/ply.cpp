#include "ply.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
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

    // Unpack directly into GaussianVertex
    model.gaussians.resize(vertexCount);
    for (int i = 0; i < vertexCount; i++) {
        GaussianVertex& v = model.gaussians[i];

        v.pos[0] = getFloat(i, "x");
        v.pos[1] = getFloat(i, "y");
        v.pos[2] = getFloat(i, "z");

        v.color[0] = 0.5f + 0.2820948f * getFloat(i, "f_dc_0"); // Constants??
        v.color[1] = 0.5f + 0.2820948f * getFloat(i, "f_dc_1");
        v.color[2] = 0.5f + 0.2820948f * getFloat(i, "f_dc_2");

        v.opacity = 1.0f / (1.0f + expf(-getFloat(i, "opacity")));

        v.scale[0] = expf(getFloat(i, "scale_0"));
        v.scale[1] = expf(getFloat(i, "scale_1"));
        v.scale[2] = expf(getFloat(i, "scale_2"));

        v.rot[0] = getFloat(i, "rot_0");
        v.rot[1] = getFloat(i, "rot_1");
        v.rot[2] = getFloat(i, "rot_2");
        v.rot[3] = getFloat(i, "rot_3");
    }

    // Free blob before returning
    blob = std::vector<uint8_t>();

    printf("Done unpacking.\n");
    return model;
}
