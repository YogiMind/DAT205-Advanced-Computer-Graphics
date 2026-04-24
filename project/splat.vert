#version 420

layout(location = 0) in vec3 position;
uniform mat4 modelViewProjectionMatrix;

out vec4 vPosition;
out mat3 vRotation;
out vec4 vScale;
out vec4 vColor;

void main()
{
    vPosition = vec4(0.0, 0.0, 5.0, 1.0); // w = opacity

    // 45 degree rotation around Z
    float a = radians(45.0);
    vRotation = mat3(
         cos(a), sin(a), 0.0,
        -sin(a), cos(a), 0.0,
         0.0,    0.0,    1.0
    );

    // Stretched: wide on X, thin on Y
    vScale = vec4(3.0, 0.5, 1.0, 0.0);

    vColor = vec4(0.2, 0.8, 1.0, 1.0); // cyan so it's easy to spot
    gl_Position = vec4(vec3(0.0), 1.0);
    gl_PointSize = 50.0;
}
// void main()
// {
// 	gl_Position = modelViewProjectionMatrix * vec4(position, 1.0);
// }
