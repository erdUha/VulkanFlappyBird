#include <fstream>
#include <sstream>

// Хэш-функция для Vertex
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& v) const noexcept {
            size_t h1 = hash<float>()(v.pos.x) ^ (hash<float>()(v.pos.y) << 1) ^ (hash<float>()(v.pos.z) << 2);
            size_t h2 = hash<float>()(v.normal.x) ^ (hash<float>()(v.normal.y) << 1) ^ (hash<float>()(v.normal.z) << 2);
            size_t h3 = hash<float>()(v.texCoord.x) ^ (hash<float>()(v.texCoord.y) << 1);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

bool LoadOBJ(const std::string& objPath, const std::string& mtlPath, std::vector<struct Vertex> &Vertices, std::vector<uint32_t> &Indices) {

    std::unordered_map<std::string, glm::vec3> temp_materials;
    
    if (mtlPath.compare("") != 0)
    {
      std::cout << "Loading \"" << mtlPath << "\"...\n";
      std::ifstream mtlFile("static/" + mtlPath);
      if (!mtlFile.is_open()) {
          //std::cerr << "Не удалось открыть OBJ файл: " << path << std::endl;
          std::cerr << "ERROR: Can't open file " << mtlPath << std::endl;
          exit(-1);
      }

      std::string currentMaterial;

      std::string mtlLine;
      while (std::getline(mtlFile, mtlLine)) {
        std::istringstream iss(mtlLine);
        std::string type;
        iss >> type;

        if (type == "newmtl")
        {
          iss >> currentMaterial;
        }
        else if (type == "Kd")
        {
          glm::vec3 color;
          iss
            >> color.r
            >> color.g
            >> color.b;
          temp_materials[currentMaterial] = color;
        }
      }
      mtlFile.close();
    }

    std::cout << "Loading \"" << objPath << "\"...\n";
    std::ifstream objFile("static/" + objPath);
    if (!objFile.is_open()) {
        //std::cerr << "Не удалось открыть OBJ файл: " << path << std::endl;
        std::cerr << "ERROR: Can't open file " << objPath << std::endl;
        exit(-1);
    }

    std::vector<glm::vec3> temp_positions;
    std::vector<glm::vec2> temp_texcoords;
    std::vector<glm::vec3> temp_normals;
    std::vector<glm::vec3> temp_colors;

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    std::string currentMaterial = "";
    std::string objLine;
    while (std::getline(objFile, objLine)) {
        std::istringstream iss(objLine);
        std::string type;
        iss >> type;

        if (type == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            temp_positions.push_back(pos);
        }
        else if (type == "vt") {
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            temp_texcoords.push_back(uv);
        }
        else if (type == "vn") {
            glm::vec3 n;
            iss >> n.x >> n.y >> n.z;
            temp_normals.push_back(n);
        }
        else if (type == "usemtl")
        {
            iss >> currentMaterial;
        }
        else if (type == "f") {
            std::string vertexDef;
            std::vector<uint32_t> faceIndices;

            while (iss >> vertexDef) {
                int vIdx = 0, tIdx = 0, nIdx = 0;

                // заменить '/' на пробел
                for (char& c : vertexDef) if (c == '/') c = ' ';
                std::istringstream viss(vertexDef);
                viss >> vIdx >> tIdx >> nIdx;

                Vertex vert{};
                vert.pos = (vIdx > 0) ? temp_positions[vIdx - 1] : glm::vec3(0.0f);
                vert.texCoord = (tIdx > 0) ? temp_texcoords[tIdx - 1] : glm::vec2(0.0f);
                vert.normal = (nIdx > 0) ? temp_normals[nIdx - 1] : glm::vec3(0.0f);
                if (currentMaterial.compare("") != 0)
                  vert.color = temp_materials[currentMaterial];
                else
                  vert.color = glm::vec3(1.0f);

                if (uniqueVertices.count(vert) == 0) {
                    uniqueVertices[vert] = static_cast<uint32_t>(Vertices.size());
                    Vertices.push_back(vert);
                }
                faceIndices.push_back(uniqueVertices[vert]);
            }

            // триангуляция: делаем из n-угольника треугольники
            for (size_t i = 1; i + 1 < faceIndices.size(); i++) {
                Indices.push_back(faceIndices[0]);
                Indices.push_back(faceIndices[i]);
                Indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    objFile.close();

    return true;
}

