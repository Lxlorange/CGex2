#version 330 core

out vec4 FragColor;

in VS_OUT {
    vec3 normal;
    vec3 worldPos;
    vec2 texCoord;
    mat3 tbn;
} fsIn;

uniform bool uHasTexture;
uniform bool uHasNormalMap;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_normal1;
uniform vec3 uMaterialDiffuse;
uniform vec3 uMaterialEmissive;
uniform vec3 uLightDirection;
uniform vec3 uViewPosition;
uniform bool uLightOn;
uniform float uAmbientStrength;
uniform vec3 uAmbientColor;

void main()
{
    vec3 baseColor = uMaterialDiffuse;
    if (uHasTexture) {
        baseColor = texture(texture_diffuse1, fsIn.texCoord).rgb;
    }

    vec3 N = normalize(fsIn.normal);
    if (uHasNormalMap) {
        vec3 tangentNormal = texture(texture_normal1, fsIn.texCoord).xyz * 2.0 - 1.0;
        N = normalize(fsIn.tbn * tangentNormal);
    }

    vec3 ambient = uAmbientStrength * uAmbientColor * baseColor;

    if (uLightOn) {
        vec3 L = normalize(-uLightDirection);
        float diffuseFactor = max(dot(N, L), 0.0);
        vec3 diffuse = diffuseFactor * baseColor;

        vec3 V = normalize(uViewPosition - fsIn.worldPos);
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0);
        vec3 specular = vec3(0.2) * spec;

        FragColor = vec4(ambient + diffuse + specular + uMaterialEmissive, 1.0);
    } else {
        FragColor = vec4(ambient, 1.0);
    }
}
