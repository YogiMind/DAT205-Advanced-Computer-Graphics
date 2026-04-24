#version 420

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 position_opacity;
in vec4 rotation;
in vec4 scale;
in vec4 color;

uniform cameraPos;

void main()
{

    vec4 center = gl_in[0].gl_Position;

    vec2 offsets[4] = vec2[](
        vec2(-1, -1),
        vec2(1, -1),
        vec2(-1, 1),
        vec2(1, 1)
    );

    float size = 0.1;
    
    for(int i = 0; i < 4; i++) 
    {
        gl_Position = center + vec4(offsets[i] * size, 0.0, 0.0);
        EmitVertex();
    }
    
    EndPrimitive();
}


