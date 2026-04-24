#version 420


layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform vec3 cameraPos; // unused
uniform int viewportWidth;
uniform int viewportHeight;

in vec4 vPosition[];
in vec4 vScale[];
in vec4 vColor[];
in mat3 vRotation[];

out vec2 gCenter; // maybe not needed?
out mat2 invCov;
out vec2 gUV;
out vec4 gColor;
out float gOpacity;

// concider moving some parts to vertex shader?
void main()
{
    vec3 center_world = vPosition[0].xyz;
    float opacity = vPosition[0].w;
    vec3 scale = vScale[0].xyz;

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
    float z = p.z; // camera dir, do i need to transform other coords?

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

    // Safe inverse
    // if (determinant(cov_screen) < 1e-6) return; // exploding term
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
