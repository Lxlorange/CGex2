#version 330 core

out float FragColor;

in vec2 TexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;
uniform vec3 uSamples[64];
uniform mat4 uProjection;
uniform vec2 uNoiseScale;
uniform float uRadius;
uniform float uBias;

void main()
{
    vec3 fragPos = texture(gPosition, TexCoord).xyz;
    vec3 normal = normalize(texture(gNormal, TexCoord).xyz * 2.0 - 1.0);
    if (length(normal) < 0.001) {
        FragColor = 1.0;
        return;
    }

    vec3 randomVec = normalize(texture(texNoise, TexCoord * uNoiseScale).xyz);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 64; ++i) {
        vec3 samplePos = fragPos + tbn * uSamples[i] * uRadius;

        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= max(offset.w, 0.0001);
        offset.xyz = offset.xyz * 0.5 + 0.5;
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }

        float sampleDepth = texture(gPosition, offset.xy).z;
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / max(abs(fragPos.z - sampleDepth), 0.0001));
        occlusion += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    float ao = 1.0 - occlusion / 64.0;
    FragColor = clamp(ao, 0.0, 1.0);
}
