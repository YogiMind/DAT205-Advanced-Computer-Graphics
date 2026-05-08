#version 420


layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform int viewportWidth;
uniform int viewportHeight;

in vec3 vPosition[];
in vec3 vScale[];
in vec4 vColor[];
in mat3 vRotation[];

out vec2 gUV;
flat out vec2 gCenter; // maybe not needed?
flat out vec4 gColor;
flat out float gOpacity;
flat out mat2 invCov;

void main()
{
    vec3 center_world = vPosition[0];
    float opacity = vColor[0].w;
    vec3 scale = vScale[0];

    mat3 R = vRotation[0];
    mat3 S = mat3(scale.x, 0.0, 0.0, 
                0.0, scale.y, 0.0,
                0.0, 0.0, scale.z);
    mat3 cov_world = R * S * S * transpose(R);

    // Convert to view space
    mat3 W = mat3(viewMatrix);
    mat3 cov_view = W * cov_world * transpose(W);

    // Position in viewspace
    vec3 p = (viewMatrix * vec4(center_world, 1.0)).xyz;
    float x = p.x;
    float y = p.y;
    float z = p.z;

    // if (z <= 0.0) return;

    // Jacobian
    float fx = projectionMatrix[0][0];
    float fy = projectionMatrix[1][1];

    mat3x2 J = transpose(mat2x3(
        fx / z, 0.0, -fx * x / (z * z),
        0.0, fy / z, -fy * y / (z * z)
    ));

    // Convert to screen space
    mat2 cov_screen = J * cov_view * transpose(J);

    vec2 vp = vec2(viewportWidth, viewportHeight);
    mat2 cov_pixels = mat2(
        cov_screen[0][0] * (vp.x * 0.5) * (vp.x * 0.5),
        cov_screen[0][1] * (vp.x * 0.5) * (vp.y * 0.5),
        cov_screen[1][0] * (vp.x * 0.5) * (vp.y * 0.5),
        cov_screen[1][1] * (vp.y * 0.5) * (vp.y * 0.5)
    );

    // radius cull
    float r = 3.0 * sqrt(max(cov_pixels[0][0], cov_pixels[1][1]));
    if (r < 1.0) return;
    if (determinant(cov_pixels) < 1e-3) return;

    mat2 cov_inverse = inverse(cov_pixels);

    // Project center
    vec4 center_screen = projectionMatrix * viewMatrix * vec4(center_world, 1.0);

    // Quad
    vec2 offsets[4] = vec2[](
        vec2(-1, -1),
        vec2( 1, -1),
        vec2(-1,  1),
        vec2( 1,  1)
    );

    float radius = 3.0 * sqrt(max(cov_screen[0][0], cov_screen[1][1])); // 3.0 is Gaussian cutoff

    // Currently not rotating the quad
    for(int i = 0; i < 4; i++)
    {
        vec2 offset = offsets[i] * radius;

        vec4 pos = center_screen;
        pos.xy += offset * center_screen.w;

        gl_Position = pos;

        gUV = offsets[i]; // normalized
        gColor = vColor[0];
        gOpacity = opacity;
        invCov = cov_inverse;

        vec2 ndc = center_screen.xy / center_screen.w;
        gCenter = (ndc * 0.5 + 0.5) * vec2(viewportWidth, viewportHeight);
        EmitVertex();
    }

    EndPrimitive();
}
