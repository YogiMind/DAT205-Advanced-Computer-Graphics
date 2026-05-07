#version 420

in vec2 gUV;
flat in vec2 gCenter;
flat in vec4 gColor;
flat in float gOpacity;
flat in mat2 invCov;

out vec4 fragmentColor;

void main()
{
    vec2 d = (gl_FragCoord.xy - gCenter);
    // vec2 d = gUV * radius;
    float exponent = dot(d, invCov * d);

    // if (exponent > 9.0) 
    //     discard;

    float weight = exp(-0.5 * exponent);

    float alpha = gOpacity * weight;
    vec3 color = gColor.rgb * alpha;

    fragmentColor = vec4(color, alpha);
}
