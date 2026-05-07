#pragma once
#include <vector>
#include <string>

struct Gaussian {
    float x, y, z;
    float f_dc[3];
    float opacity;
    float scale[3];
    float rot[4];
};

struct GaussianVertex {
    float pos[3];
    float color[3];
    float opacity;
    float scale[3];
    float rot[4];
};

struct PLYModel {
    std::vector<GaussianVertex> gaussians;
};

PLYModel loadPLY(const std::string& path);
