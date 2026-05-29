#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uScene;
uniform float uThreshold;

void main()
{
    vec3 color = texture(uScene, TexCoord).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 bright = brightness > uThreshold ? color : vec3(0.0);
    FragColor = vec4(bright, 1.0);
}
