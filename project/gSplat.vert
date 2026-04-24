#version 420

layout(location = 0) in vec4 position_opacity;
layout(location = 1) in mat3 rotation;
layout(location = 2) in vec4 scale;
layout(location = 3) in vec4 color;

out vec4 vPosition;
out mat3 vRotation;
out vec4 vScale;
out vec4 vColor;

void main()
{
    vPosition = position_opacity;
    vRotation = rotation;
    vScale = scale;
    vColor = color;

    gl_Position = vec4(position_opacity.xyz, 1.0); // temporary
}
