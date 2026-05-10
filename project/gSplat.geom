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

out vec2 gPixelOffset; // pixel space offset from center
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

    vec4 p_view = viewMatrix * vec4(center_world, 1.0);
    // Position in viewspace
    vec3 p = p_view.xyz;
    float x = p.x;
    float y = p.y;
    float z = p.z;

    // Jacobian
    float fx = projectionMatrix[0][0];
    float fy = projectionMatrix[1][1];

    mat3x2 J = transpose(mat2x3(
        fx / z, 0.0, -fx * x / (z * z),
        0.0, fy / z, -fy * y / (z * z)
    ));

    vec2 vp = vec2(viewportWidth, viewportHeight);

    // Convert to screen space
    mat2 cov_screen = J * cov_view * transpose(J);

    mat2 cov_pixels = mat2(
        cov_screen[0][0] * (vp.x * 0.5) * (vp.x * 0.5),
        cov_screen[0][1] * (vp.x * 0.5) * (vp.y * 0.5),
        cov_screen[1][0] * (vp.x * 0.5) * (vp.y * 0.5),
        cov_screen[1][1] * (vp.y * 0.5) * (vp.y * 0.5)
    );

    float r = 3.0 * sqrt(max(cov_pixels[0][0], cov_pixels[1][1]));
    if (r > min(vp.x, vp.y) * 2.0) return; // cull if larger than screen
    if (determinant(cov_pixels) < 1e-5) return;

    mat2 cov_inverse = inverse(cov_pixels);

    // Project center
    vec4 center_screen = projectionMatrix * p_view;

    // get eigenvalues and compute oriented quad
    float a = cov_pixels[0][0], b = cov_pixels[0][1], d = cov_pixels[1][1];
    float trace = a + d;
    float det = a*d - b*b;
    float sqrtTerm = sqrt(max(0.0, trace*trace*0.25 - det));
    float lambda1 = trace*0.5 + sqrtTerm;
    float lambda2 = trace*0.5 - sqrtTerm;
    float r1 = 3.0 * sqrt(abs(lambda1));
    float r2 = 3.0 * sqrt(abs(lambda2));

    vec2 axis = (abs(b) < 1e-6) ? vec2(1,0) : normalize(vec2(b, lambda1 - a));
    vec2 perp = vec2(-axis.y, axis.x);

    // Quad
    vec2 corners[4] = vec2[](
            -axis*r1 - perp*r2,
            axis*r1 - perp*r2,
            -axis*r1 + perp*r2,
            axis*r1 + perp*r2
            );


    gColor = vColor[0];
    gOpacity = opacity;
    invCov = cov_inverse;

    for(int i = 0; i < 4; i++)
    {
        vec2 pixelOffset = corners[i];
        gPixelOffset = pixelOffset;

        // convert pixel offset to NDC offset
        vec2 ndcOffset = pixelOffset / (vp * 0.5);
        vec4 pos = center_screen;
        pos.xy += ndcOffset * center_screen.w;
        gl_Position = pos;

        EmitVertex();
    }

    EndPrimitive();
}
