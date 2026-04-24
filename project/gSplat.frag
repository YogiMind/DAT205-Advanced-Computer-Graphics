#version 420

in vec2 gUV;
in vec4 gColor;
in float gOpacity;
in mat2 invCov;
in vec2 gCenter;

out vec4 fragmentColor;

void main()
{
    // fragmentColor = vec4(1.0, 0.0, 0.0, 1.0); // flat color, ignore Gaussian

    vec2 d = (gl_FragCoord.xy - gCenter);
    // vec2 d = gUV * radius;
    float exponent = dot(d, invCov * d);

    if (exponent > 9.0) 
        discard;

    float weight = exp(-0.5 * exponent);

    float alpha = gOpacity * weight;
    vec3 color = gColor.rgb * alpha;

    fragmentColor = vec4(color, alpha);
}
