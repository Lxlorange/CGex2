#version 330 core

out float FragColor;

in vec2 TexCoord;

uniform sampler2D uSSAOInput;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uSSAOInput, 0));
    float result = 0.0;
    for (int x = -2; x <= 1; ++x) {
        for (int y = -2; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(uSSAOInput, TexCoord + offset).r;
        }
    }
    FragColor = result / 16.0;
}
