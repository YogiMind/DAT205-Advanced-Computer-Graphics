#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in float opacity;
layout(location = 3) in vec3 scale;
layout(location = 4) in vec4 rotation;


out vec3 vPosition;
out mat3 vRotation;
// out vec3 vRotCol0;
// out vec3 vRotCol1;
// out vec3 vRotCol2;
out vec3 vScale;
out vec4 vColor;

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

void main()
{
    vPosition = position;

    vRotation = quatToMat3(rotation); 
    // vRotCol0 = vRotation[0];
    // vRotCol1 = vRotation[1];
    // vRotCol2 = vRotation[2];

    vScale = scale;
    vColor = vec4(color, opacity);

    gl_Position = vec4(position, 1.0); // temporary
}
