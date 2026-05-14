#version 330 core

out vec4 FragColor;

in VS_OUT {
    vec3 normal;
    vec3 worldPos;
    vec2 texCoord;
} fsIn;

uniform bool uHasTexture;
uniform sampler2D texture_diffuse1;
uniform vec3 uFallbackColor;
uniform vec3 uLightDirection;
uniform vec3 uViewPosition;

void main()
{
    vec3 baseColor = uFallbackColor;
    if (uHasTexture) {
        baseColor = texture(texture_diffuse1, fsIn.texCoord).rgb;
    }

    vec3 N = normalize(fsIn.normal);
    vec3 L = normalize(-uLightDirection);
    float diffuseFactor = max(dot(N, L), 0.0);

    vec3 ambient = 0.25 * baseColor;
    vec3 diffuse = diffuseFactor * baseColor;

    vec3 V = normalize(uViewPosition - fsIn.worldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = vec3(0.2) * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
