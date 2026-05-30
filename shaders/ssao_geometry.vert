#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out VS_OUT {
    vec3 viewPos;
    vec3 viewNormal;
} vsOut;

void main()
{
    vec4 viewPosition = uView * uModel * vec4(aPos, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(uView * uModel)));
    vsOut.viewPos = viewPosition.xyz;
    vsOut.viewNormal = normalize(normalMatrix * aNormal);
    gl_Position = uProjection * viewPosition;
}
