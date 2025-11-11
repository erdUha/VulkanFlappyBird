#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec3 fragMaterialSpecular;
layout(location = 3) out vec3 fragViewNormal;
layout(location = 4) out vec3 fragPosition;
layout(location = 5) out vec3 fragViewVec;
layout(location = 6) out vec2 fragTexCoord;
layout(location = 7) out vec3 fragLightDir;
layout(location = 8) out vec4 fragLightSpacePos;
layout(location = 9) out float fragShadowMapResolution;
layout(location = 10) out float fragBiasFactor;

layout(binding = 0) uniform SceneUBO { // TODO: create another buffer for fragment shader
	mat4 model;
	mat4 view;
	mat4 proj;
  mat4 normalMatrix;
  mat4 normalViewMatrix;
  mat4 lightSpaceMatrix;
  vec3 materialSpecular; 
  vec3 lightDir;
  vec3 viewPos;
  vec3 shadowMapResolution;
  vec3 biasFactor;
} ubo;

void main()
{
  mat4 mvp = ubo.proj * ubo.view * ubo.model;
  //mvp = ubo.lightSpaceMatrix * ubo.model;
  
	gl_Position = mvp * vec4(inPosition, 1.0f);

  vec4 lightSpacePos = ubo.lightSpaceMatrix * vec4(inPosition, 1.0f);
  lightSpacePos.xyz /= lightSpacePos.w;
  lightSpacePos.xy = lightSpacePos.xy * 0.5f + 0.5f;
  fragShadowMapResolution = ubo.shadowMapResolution.x;
  fragBiasFactor = ubo.biasFactor.x;

  fragLightSpacePos = lightSpacePos;
  fragLightDir = ubo.lightDir;

  fragPosition = vec3(ubo.model * vec4(inPosition, 1.0f));
  fragTexCoord = inTexCoord;

  fragColor = inColor;
  fragMaterialSpecular = ubo.materialSpecular;

  fragNormal = vec3(ubo.normalMatrix * vec4(inNormal, 0.0f));
  fragViewVec = ubo.viewPos - fragPosition;

  fragViewNormal = vec3(normalize(ubo.normalViewMatrix * vec4(inNormal, 0.0f))); // useless
}
