#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uShadowMatrix;

out vec3 WorldPos;

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    WorldPos = world.xyz;
    gl_Position = uShadowMatrix * world;
}
