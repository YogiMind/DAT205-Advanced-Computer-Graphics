#version 420

flat in vec4 gColor;
flat in float gOpacity;
flat in mat2 invCov;
in vec2 gPixelOffset;

out vec4 fragmentColor;

void main()
{
    float exponent = dot(gPixelOffset, invCov * gPixelOffset);
    if (exponent > 9.0) discard;

    float weight = exp(-0.5 * exponent);
    float alpha = gOpacity * weight;
    if (alpha < 1.0/255.0) discard;

    fragmentColor = vec4(gColor.rgb, alpha);
}
