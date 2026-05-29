#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uImage;
uniform bool uHorizontal;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uImage, 0));
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(uImage, TexCoord).rgb * weights[0];

    for (int i = 1; i < 5; ++i) {
        vec2 offset = uHorizontal ? vec2(texelSize.x * float(i), 0.0) : vec2(0.0, texelSize.y * float(i));
        result += texture(uImage, TexCoord + offset).rgb * weights[i];
        result += texture(uImage, TexCoord - offset).rgb * weights[i];
    }

    FragColor = vec4(result, 1.0);
}
