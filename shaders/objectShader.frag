#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragMaterialSpecular;
layout(location = 3) in vec3 fragViewNormal;//useless
layout(location = 4) in vec3 fragPosition;
layout(location = 5) in vec3 fragViewVec;
layout(location = 6) in vec2 fragTexCoord;
layout(location = 7) in vec3 fragLightDir;
layout(location = 8) in vec4 fragLightSpacePos;
layout(location = 9) in float fragShadowMapResolution;
layout(location = 10) in float fragBiasFactor;

layout(binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform sampler2DShadow shadowMap;

layout(location = 0) out vec4 outColor;

void main()
{
  vec3 norm = normalize(fragNormal);
  vec3 viewVec = normalize(fragViewVec);

  vec3 lightVec = normalize(fragLightDir);
  vec3 halfVec = normalize(lightVec + viewVec);

  float shininess = 16.0f;
  vec3 sunIntensity = vec3(1.5f);

  vec3 diffuseLight = fragColor * sunIntensity * vec3(0.96f, 0.86f, 0.61f) * max(dot(norm, lightVec), 0.0f) * vec3(1.0f);
  vec3 specularLight = fragMaterialSpecular * sunIntensity * pow(max(dot(norm, halfVec), 0.0f), shininess) * vec3(1.0f);

  // shadow map utilization
  float bias = 0.01f * fragBiasFactor;

  float shadow = 0.0f;
  float texelSize = 1.0f / fragShadowMapResolution; // shadow map size

  for (int x = -2; x <= 2; ++x)
  for (int y = -2; y <= 2; ++y) {
      vec2 offset = vec2(x, y) * texelSize;
      shadow += texture(shadowMap, vec3(fragLightSpacePos.xy + offset, fragLightSpacePos.z - bias));
  }
  shadow /= 25.0f;

  vec3 ambientLight = fragColor * vec3(0.63, 0.76, 1.0f) * vec3(0.25f);

  outColor = vec4(( ( (diffuseLight * 0.7f) + (specularLight * 0.7f) ) * shadow) + ambientLight, 1.0f);
}
