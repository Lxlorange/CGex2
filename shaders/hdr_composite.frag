#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uScene;
uniform sampler2D uBloomBlur;
uniform bool uBloomEnabled;
uniform float uExposure;
uniform float uBloomStrength;

void main()
{
    vec3 hdrColor = texture(uScene, TexCoord).rgb;
    vec3 bloomColor = texture(uBloomBlur, TexCoord).rgb;
    if (uBloomEnabled) {
        hdrColor += bloomColor * uBloomStrength;
    }

    vec3 mapped = vec3(1.0) - exp(-hdrColor * uExposure);
    mapped = pow(max(mapped, vec3(0.0)), vec3(1.0 / 2.2));
    FragColor = vec4(mapped, 1.0);
}
