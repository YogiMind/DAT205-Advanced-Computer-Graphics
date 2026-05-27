#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 f_dc;
layout(location = 2) in float opacity;
layout(location = 3) in vec3 scale;
layout(location = 4) in vec4 rotation;

uniform samplerBuffer shRestData;
uniform vec3 cameraPos;
uniform int sh_degree;

out vec3 vPosition;
out mat3 vRotation;
out vec3 vScale;
out vec4 vColor;

const float SH_C0 = 0.28209479;
const float SH_C1 = 0.48860251;
const float SH_C2[5] = float[](1.09254843, -1.09254843, 0.31539157, -1.09254843, 0.54627422);
const float SH_C3[7] = float[](-0.59004358, 2.89061144, -0.45704580, 0.37317633,
                                -0.45704580, 1.44530573, -0.59004358);


// Assumes quaternion order (w, x, y, z)
mat3 quatToMat3(vec4 q) {
    q = normalize(q);
    float w = q.x, x = q.y, y = q.z, z = q.w;
    return mat3(
        1.0-2.0*(y*y+z*z),     2.0*(x*y+w*z),     2.0*(x*z-w*y),
            2.0*(x*y-w*z), 1.0-2.0*(x*x+z*z),     2.0*(y*z+w*x),
            2.0*(x*z+w*y),     2.0*(y*z-w*x), 1.0-2.0*(x*x+y*y)
    );
}

float getRest(int gaussianIdx, int coeffIdx) {
    return texelFetch(shRestData, gaussianIdx * 45 + coeffIdx).r;
}

vec3 evaluateSH(vec3 dir, vec3 dc, int idx) {
    float x = dir.x, y = -dir.y, z = -dir.z; // Coordinate system correction

    // Degree 0
    vec3 col = SH_C0 * dc;

    float xx = x*x, yy = y*y, zz = z*z, xy = x*y, xz = x*z, yz = y*z;

    if (sh_degree >= 1) {
            // Degree 1 — 3 coefficients per channel
            vec3 c1 = vec3(getRest(idx, 0),  getRest(idx, 15), getRest(idx, 30));
            vec3 c2 = vec3(getRest(idx, 1),  getRest(idx, 16), getRest(idx, 31));
            vec3 c3 = vec3(getRest(idx, 2),  getRest(idx, 17), getRest(idx, 32));

            col += SH_C1 * (-y * c1 + z * c2 - x * c3);
    }
    if (sh_degree >= 2) {
            // Degree 2 — 5 coefficients per channel
            vec3 c4 = vec3(getRest(idx, 3),  getRest(idx, 18), getRest(idx, 33));
            vec3 c5 = vec3(getRest(idx, 4),  getRest(idx, 19), getRest(idx, 34));
            vec3 c6 = vec3(getRest(idx, 5),  getRest(idx, 20), getRest(idx, 35));
            vec3 c7 = vec3(getRest(idx, 6),  getRest(idx, 21), getRest(idx, 36));
            vec3 c8 = vec3(getRest(idx, 7),  getRest(idx, 22), getRest(idx, 37));

            col += SH_C2[0] * xy               * c4;
            col += SH_C2[1] * yz               * c5;
            col += SH_C2[2] * (2.0*zz-xx-yy)  * c6;
            col += SH_C2[3] * xz               * c7;
            col += SH_C2[4] * (xx - yy)        * c8;
    }
    if (sh_degree >= 3) {
            // Degree 3 — 7 coefficients per channel
            vec3 c9  = vec3(getRest(idx, 8),  getRest(idx, 23), getRest(idx, 38));
            vec3 c10 = vec3(getRest(idx, 9),  getRest(idx, 24), getRest(idx, 39));
            vec3 c11 = vec3(getRest(idx, 10), getRest(idx, 25), getRest(idx, 40));
            vec3 c12 = vec3(getRest(idx, 11), getRest(idx, 26), getRest(idx, 41));
            vec3 c13 = vec3(getRest(idx, 12), getRest(idx, 27), getRest(idx, 42));
            vec3 c14 = vec3(getRest(idx, 13), getRest(idx, 28), getRest(idx, 43));
            vec3 c15 = vec3(getRest(idx, 14), getRest(idx, 29), getRest(idx, 44));

            col += SH_C3[0] * y*(3.0*xx-yy)       * c9;
            col += SH_C3[1] * xy*z                * c10;
            col += SH_C3[2] * y*(4.0*zz-xx-yy)   * c11;
            col += SH_C3[3] * z*(2.0*zz-3.0*xx-3.0*yy) * c12;
            col += SH_C3[4] * x*(4.0*zz-xx-yy)   * c13;
            col += SH_C3[5] * z*(xx-yy)           * c14;
            col += SH_C3[6] * x*(xx-3.0*yy)       * c15;
    }

    return max(col + 0.5, vec3(0.0));
}

void main()
{
    vec3 viewDir = normalize(position - cameraPos);

    vColor = vec4(evaluateSH(viewDir, f_dc, gl_VertexID), opacity);
    vPosition = position;
    vRotation = quatToMat3(rotation); 
    vScale = scale;

    gl_Position = vec4(position, 1.0);
}
