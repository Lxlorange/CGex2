#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out VS_OUT {
    vec3 normal;
    vec3 worldPos;
    vec2 texCoord;
} vsOut;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    vec4 worldPosition = uModel * vec4(aPos, 1.0);
    vsOut.worldPos = worldPosition.xyz;
    vsOut.normal = mat3(transpose(inverse(uModel))) * aNormal;
    vsOut.texCoord = aTexCoord;

    gl_Position = uProjection * uView * worldPosition;
}
