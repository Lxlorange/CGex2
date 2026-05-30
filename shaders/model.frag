#version 330 core

out vec4 FragColor;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform DirLight dirLight;
#define MAX_POINT_LIGHTS 10
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;
#define MAX_POINT_SHADOWS 8
uniform samplerCube pointShadowMaps[MAX_POINT_SHADOWS];
uniform int uPointShadowCount;
uniform float uPointShadowFarPlane;
uniform float uPointShadowStrength;
uniform int uPointShadowsEnabled;
uniform SpotLight spotLight;
uniform float uBulbDownwardInnerCos;
uniform float uBulbDownwardOuterCos;

uniform int uPointLightsOn;
uniform vec3 globalAmbient;

uniform sampler2D shadowMap;
uniform int uUseDirectionalShadow;
uniform vec2 uShadowTexelSize;
uniform float uShadowStrength;
uniform float uLampEmissionScale;
uniform float uEmissiveStrengthMultiplier;
uniform vec3 uSceneMin;
uniform vec3 uSceneMax;
uniform int uApplyWindowFalloff;

in VS_OUT {
    vec3 normal;
    vec3 worldPos;
    vec2 texCoord;
    mat3 tbn;
    vec4 fragPosLightSpace;
} fsIn;

uniform bool uHasTexture;
uniform bool uHasNormalMap;
uniform bool uHasEmissiveMap;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_normal1;
uniform sampler2D texture_emissive1;
uniform vec3 uMaterialDiffuse;
uniform vec3 uMaterialSpecular;
uniform float uMaterialShininess;
uniform float uMaterialAlpha;
uniform vec3 uMaterialEmissive;
uniform vec3 uViewPosition;

float attenuationPoly(float distance, float c, float kl, float kq)
{
    float denom = max(c + kl * distance + kq * (distance * distance), 0.0001);
    return 1.0 / denom;
}

float hash12(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float wallSpecAttenuation(vec3 n)
{
    float v = 1.0 - abs(n.y);
    v = smoothstep(0.1, 0.92, v);
    return mix(1.0, 0.18, v);
}

float directionalShadow(vec4 fragPosLightSpace, vec3 norm, vec3 lightDirToSun)
{
    if (uUseDirectionalShadow == 0) {
        return 1.0;
    }

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) {
        return 1.0;
    }
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    vec3 lightDir = normalize(-lightDirToSun);
    float ndotl = max(dot(norm, lightDir), 0.0);
    float bias = max(0.0009 * (1.0 - ndotl), 0.00018);

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(float(x), float(y)) * uShadowTexelSize).r;
            shadow += projCoords.z - bias > closestDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return mix(1.0 - clamp(uShadowStrength, 0.0, 1.0), 1.0, 1.0 - shadow);
}

float pointShadow(int index, vec3 fragPos, vec3 lightPos, vec3 normal)
{
    if (uPointShadowsEnabled == 0 || index >= uPointShadowCount || index >= MAX_POINT_SHADOWS) {
        return 1.0;
    }

    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    if (currentDepth >= uPointShadowFarPlane) {
        return 1.0;
    }

    vec3 lightToFrag = normalize(fragToLight);
    float ndotl = max(dot(normal, -lightToFrag), 0.0);
    float bias = max(0.055 * (1.0 - ndotl), 0.018);
    float diskRadius = 0.025 + currentDepth / uPointShadowFarPlane * 0.035;

    vec3 offsets[6] = vec3[](
        vec3( 1.0,  0.0,  0.0),
        vec3(-1.0,  0.0,  0.0),
        vec3( 0.0,  1.0,  0.0),
        vec3( 0.0, -1.0,  0.0),
        vec3( 0.0,  0.0,  1.0),
        vec3( 0.0,  0.0, -1.0)
    );

    float shadow = 0.0;
    for (int i = 0; i < 6; ++i) {
        float closestDepth = texture(pointShadowMaps[index], fragToLight + offsets[i] * diskRadius).r;
        closestDepth *= uPointShadowFarPlane;
        shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0;
    }
    shadow /= 6.0;
    return mix(1.0 - clamp(uPointShadowStrength, 0.0, 1.0), 1.0, 1.0 - shadow);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 baseColor, float shadow, float wallSpec)
{
    vec3 lightDir = normalize(-light.direction);
    float raw = max(dot(normal, lightDir), 0.0);
    float diff = pow(mix(0.12, 1.0, raw), 0.92);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), max(uMaterialShininess, 1.0));

    vec3 ambient = light.ambient * baseColor;
    vec3 diffuse = light.diffuse * diff * baseColor;
    vec3 specular = light.specular * spec * uMaterialSpecular * wallSpec * 1.25;
    return ambient + shadow * (diffuse + specular);
}

vec3 CalcPointLight(PointLight light, int lightIndex, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 baseColor, float wallSpec)
{
    vec3 toLight = light.position - fragPos;
    float distance = length(toLight);
    vec3 lightDir = normalize(toLight);

    float attenuation = 1.0 / (distance * distance + 0.001);
    vec3 fromLightToFrag = -lightDir;
    float downward = dot(fromLightToFrag, vec3(0.0, -1.0, 0.0));
    float shadeLimit = smoothstep(uBulbDownwardOuterCos, uBulbDownwardInnerCos, downward);
    attenuation *= mix(0.18, 1.0, shadeLimit);
    float shadow = pointShadow(lightIndex, fragPos, light.position, normal);

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), max(uMaterialShininess, 1.0));

    vec3 ambient = light.color * 0.025 * baseColor * attenuation;
    vec3 diffuse = light.color * diff * baseColor * attenuation;
    vec3 specular = light.color * spec * uMaterialSpecular * wallSpec * attenuation;
    return ambient + shadow * (diffuse + specular);
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 baseColor, float wallSpec)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), max(uMaterialShininess, 1.0));

    float distance = length(light.position - fragPos);
    float attenuation = pow(attenuationPoly(distance, light.constant, light.linear, light.quadratic), 1.35);

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = max(light.cutOff - light.outerCutOff, 0.0001);
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambient = light.ambient * baseColor * attenuation * intensity;
    vec3 diffuse = light.diffuse * diff * baseColor * attenuation * intensity;
    vec3 specular = light.specular * spec * uMaterialSpecular * wallSpec * attenuation * intensity;
    return ambient + diffuse + specular;
}

void main()
{
    vec3 baseColor = uMaterialDiffuse;
    float alpha = uMaterialAlpha;
    if (uHasTexture) {
        vec4 texColor = texture(texture_diffuse1, fsIn.texCoord);
        baseColor = pow(texColor.rgb, vec3(2.2));
        alpha *= texColor.a;
    }

    vec3 norm = normalize(fsIn.normal);
    if (uHasNormalMap) {
        vec3 tangentNormal = texture(texture_normal1, fsIn.texCoord).xyz * 2.0 - 1.0;
        norm = normalize(fsIn.tbn * tangentNormal);
    }
    vec3 viewDir = normalize(uViewPosition - fsIn.worldPos);
    float wallSpec = wallSpecAttenuation(norm);

    float h = hash12(fsIn.worldPos.xz * 3.17 + fsIn.worldPos.y);
    float vertWall = smoothstep(0.22, 0.96, 1.0 - abs(norm.y));
    baseColor *= mix(vec3(1.0), vec3(0.97, 0.98, 1.0), vertWall * (0.5 + 0.5 * h) * 0.07);

    float lum = dot(baseColor, vec3(0.299, 0.587, 0.114));
    if (lum < 0.14 && vertWall > 0.25) {
        baseColor += (h - 0.5) * 0.018;
    }

    if (baseColor.r > 0.42 && baseColor.g < 0.22 && baseColor.b < 0.22) {
        baseColor = pow(max(baseColor, vec3(0.001)), vec3(0.9)) * vec3(0.9, 0.88, 0.88);
    }

    if (lum > 0.93 && vertWall > 0.2) {
        baseColor *= 0.92 + 0.06 * h;
    }

    vec3 towardSun = normalize(dirLight.direction);
    float dirShadow = directionalShadow(fsIn.fragPosLightSpace, norm, towardSun);

    vec3 windowSkylight = vec3(0.0);
    if (uApplyWindowFalloff == 1) {
        vec3 inc = -towardSun;
        float ext = max(abs(dot(uSceneMax - uSceneMin, inc)), 0.02);
        float tDep = dot(fsIn.worldPos - uSceneMin, inc) / ext;
        tDep = clamp(tDep, 0.0, 1.0);
        float nearBeam = clamp(1.0 - smoothstep(0.0, 0.55, tDep), 0.0, 1.0);
        float facing = max(dot(norm, inc), 0.0);
        float vert = smoothstep(0.18, 0.94, 1.0 - abs(norm.y));
        windowSkylight = vec3(0.42, 0.55, 0.68) * facing * vert * (nearBeam * nearBeam) * 0.22;
    }

    vec3 result = globalAmbient * baseColor;
    result += CalcDirLight(dirLight, norm, viewDir, baseColor, dirShadow, wallSpec);
    result += windowSkylight * baseColor * mix(0.35, 1.0, dirShadow);

    if (uApplyWindowFalloff == 1) {
        vec3 inc = -normalize(dirLight.direction);
        float ndotv = clamp(dot(norm, viewDir), 0.0, 1.0);
        float rim = pow(1.0 - ndotv, 2.4) * max(dot(norm, inc), 0.0);
        result += rim * baseColor * vec3(0.12, 0.13, 0.15) * mix(0.4, 1.0, dirShadow);
    }

    if (uPointLightsOn == 1) {
        for (int i = 0; i < numPointLights; ++i) {
            result += CalcPointLight(pointLights[i], i, norm, fsIn.worldPos, viewDir, baseColor, wallSpec);
        }
    }

    result += CalcSpotLight(spotLight, norm, fsIn.worldPos, viewDir, baseColor, wallSpec);
    vec3 rawEmissive = max(uMaterialEmissive, vec3(0.0));
    if (uHasEmissiveMap) {
        rawEmissive *= pow(texture(texture_emissive1, fsIn.texCoord).rgb, vec3(2.2));
    }
    float emissionScale = max(uLampEmissionScale * uEmissiveStrengthMultiplier, 0.0);
    vec3 emissive = rawEmissive * emissionScale;
    result += emissive * 5.0;

    FragColor = vec4(max(result, vec3(0.0)), alpha);
}
