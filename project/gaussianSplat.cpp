#include <glm/glm.hpp>
#include <vector>
using namespace glm;

struct gaussianSplat
{
    vec3 position;  // world-space mean

    vec3 scale;     // standard deviations along axes
    vec4 rotation;  // quaternion

    vec3 color;     // RGB
    float opacity;  // alpha
};

struct SplatGPU
{
    std::vector<vec4> position; // xyz + opacity
    std::vector<vec4> rotation; // quarternion
    std::vector<vec4> scale; // xyz + padding
    std::vector<vec4> color; // rgb + unused
};

// struct GaussianSplatCloud
// {
//     std::vector<vec4> position_opacity; // xyz + alpha
//     std::vector<vec4> scale_rotation0; // xyz + rot.x
//     std::vector<vec4> rotation1_color; // rot.yzw + r
//     std::vector<vec4> color_padding; // g b padding
// };
