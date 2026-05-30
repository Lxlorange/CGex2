#version 330 core

in vec3 WorldPos;

uniform vec3 uLightPos;
uniform float uFarPlane;

void main()
{
    float dist = length(WorldPos - uLightPos);
    gl_FragDepth = dist / uFarPlane;
}
