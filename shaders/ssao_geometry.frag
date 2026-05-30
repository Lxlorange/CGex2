#version 330 core

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;

in VS_OUT {
    vec3 viewPos;
    vec3 viewNormal;
} fsIn;

void main()
{
    gPosition = vec4(fsIn.viewPos, 1.0);
    gNormal = vec4(normalize(fsIn.viewNormal) * 0.5 + 0.5, 1.0);
}
