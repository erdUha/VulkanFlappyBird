struct Vertex {
  glm::vec3 pos;
	glm::vec3 color;
	glm::vec3 normal;
  glm::vec2 texCoord;

  bool operator==(const Vertex& other) const {
      return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
  }
};

#include "obj_loader.hxx"

struct Model {
  std::string name = "Unnamed Model";

  std::string objPath = "";
  std::string mtlPath = "";

  std::vector<struct Vertex> vertices;
  std::vector<uint32_t> indices;

  VkBuffer vertexBuffer;
  VkDeviceMemory vertexBufferMemory;

  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;
};

std::unordered_map<std::string, struct Model> Models;

struct GameObject {
  bool isBad = false;
  bool isScored = false;
  std::string name = "Unnamed Object";

  std::string modelName = "Unnamed Model";

  glm::vec3 accel = glm::vec3(0.0f);
  glm::vec3 velocity = glm::vec3(0.0f);
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 rotationSpeed = glm::vec3(0.0f);
  glm::vec3 rotation = glm::vec3(0.0f);
  glm::vec3 scale = glm::vec3(1.0f);

  std::vector<VkBuffer> shadowMapUniformBuffers;
  std::vector<VkDeviceMemory> shadowMapUniformBuffersMemory;
  std::vector<void*> shadowMapUniformBuffersMapped;

  std::vector<VkBuffer> uniformBuffers;
  std::vector<VkDeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;

  std::vector<VkDescriptorSet> shadowMapDescriptorSets;
  std::vector<VkDescriptorSet> descriptorSets;

  glm::mat4 getModelMatrix() const
  {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);
    return model;
  }
};

std::vector<struct GameObject> gameObjects;
std::queue<struct GameObject> spawnQueue;
std::queue<struct GameObject> despawnQueue;

const struct GameObject tubesTemplate = {
  .isBad = true,
  .name = "Pair of Tubes",
  .modelName = "Tubes Model",
  .velocity = glm::vec3(-0.015f, 0.0f, 0.0f),
  .scale = glm::vec3(1.5f, 1.5f, 1.20f)
};


void loadModels()
{
  struct Model flappyBirdModel;
  flappyBirdModel.name = "Flappy Bird Model";
  flappyBirdModel.objPath = "obj/flappyBird.obj";
  flappyBirdModel.mtlPath = "obj/flappyBird.mtl";
  Models[flappyBirdModel.name] = flappyBirdModel;

  struct Model tubesModel;
  tubesModel.name = "Tubes Model";
  tubesModel.objPath = "obj/tubes.obj";
  tubesModel.mtlPath = "obj/tubes.mtl";
  Models[tubesModel.name] = tubesModel;

  struct Model terrainModel;
  terrainModel.name = "Terrain Model";
  terrainModel.objPath = "obj/terrain.obj";
  terrainModel.mtlPath = "obj/terrain.mtl";
  Models[terrainModel.name] = terrainModel;

  for (auto& model : Models)
  {
    LoadOBJ(
        model.second.objPath,
        model.second.mtlPath,
        model.second.vertices,
        model.second.indices
      );
  }
}

const float numberOfTubes = 15;

void createObjects()
{
  struct GameObject FlappyBird;
  FlappyBird.name = "FlappyBird";
  FlappyBird.modelName = "Flappy Bird Model";
  FlappyBird.position = glm::vec3(0.0f, 0.0f, 10.0f);
  FlappyBird.rotation.z = 90.0f;
  FlappyBird.scale = glm::vec3(1.0f);
  FlappyBird.accel.z = -0.0003f;
  gameObjects.push_back(FlappyBird);

  struct GameObject Terrain;
  Terrain.name = "Terrain";
  Terrain.modelName = "Terrain Model";
  Terrain.position = glm::vec3(0.0f, 0.0f, 0.0f);
  Terrain.scale = glm::vec3(100.0f, 10.0f, 1.0f);
  gameObjects.push_back(Terrain);

  for (int i = 0; i < numberOfTubes; i++)
  {
    struct GameObject tubes = tubesTemplate;
    tubes.position = glm::vec3(
        30.0f + static_cast<float>(i) * (200.0f / numberOfTubes),
        0.0f,
        6.0f + 10.5f * 0.01f * static_cast<float>(rand() % 100));
    gameObjects.push_back(tubes);
  }

  //LoadOBJ("obj/suzanne.obj", m.vertices, m.indices);
  //std::cout << "Number of vetices: " << m.vertices.size() << "\n";
  //std::cout << "Number of indices: " << m.indices.size() << "\n";
}


static VkVertexInputBindingDescription getBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(struct Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

const uint32_t attributeDescriptionCount = 4;

static VkVertexInputAttributeDescription* getAttributeDescriptions()
{
	VkVertexInputAttributeDescription* attributeDescriptions = new VkVertexInputAttributeDescription[4]{};
	// vec3 inPosition
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(struct Vertex, pos);
	// vec3 inColor
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(struct Vertex, color);
	// vec3 inNormal
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(struct Vertex, normal);
  // vec2 inTexCoord
	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[3].offset = offsetof(struct Vertex, texCoord);
	return attributeDescriptions;
}
