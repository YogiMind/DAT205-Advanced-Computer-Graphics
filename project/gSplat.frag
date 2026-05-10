#version 420

in vec2 gUV;
flat in vec2 gCenter;
flat in vec4 gColor;
flat in float gOpacity;
flat in mat2 invCov;
flat in float gMaxRadius;

out vec4 fragmentColor;

void main()
{
    vec2 d = (gl_FragCoord.xy - gCenter);
    // cheap radius check before matrix multiply
    // if (dot(d, d) > 9.0 * gMaxRadius * gMaxRadius) discard;

    float exponent = dot(d, invCov * d);
    if (exponent > 9.0) discard;

    float weight = exp(-0.5 * exponent);
    float alpha = gOpacity * weight;
    if (alpha < 1.0/255.0) discard;

    fragmentColor = vec4(gColor.rgb, alpha);
}
