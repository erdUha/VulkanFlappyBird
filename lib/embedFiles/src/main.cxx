#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>

#include "base64.hxx"

std::string readFile(const std::string filePath);
void writeFile(const std::string filePath, std::unordered_map<std::string, std::string>& texts);

int main (int argc, char** argv)
{
  std::string objectVertShader = readFile("shaders/objectShader.vert.spv");
  std::string objectFragShader = readFile("shaders/objectShader.frag.spv");
  std::string shadowMapVertShader = readFile("shaders/shadowMapShader.vert.spv");
  std::string shadowMapFragShader = readFile("shaders/shadowMapShader.frag.spv");
  std::unordered_map<std::string, std::string> texts;
  texts["objectVertShaderCodeBase64"] = base64_encode(objectVertShader, false);
  texts["objectFragShaderCodeBase64"] = base64_encode(objectFragShader, false);
  texts["shadowMapVertShaderCodeBase64"] = base64_encode(shadowMapVertShader, false);
  texts["shadowMapFragShaderCodeBase64"] = base64_encode(shadowMapFragShader, false);
  writeFile("src/lib/embededFiles.hxx", texts);
  return 0;
}

std::string readFile(const std::string filePath)
{
  std::ifstream file;
  file.open(filePath, std::ios::binary);
  if (!file)
  {
		printf("\033[31mERR:\033[0m Failed to open file \"%s\"\n", filePath.data());
		exit(-1);
  }
  file.seekg(0, std::ios::end);
  size_t fileSizeInByte = file.tellg();
  std::string buffer;
  buffer.resize(fileSizeInByte);
  file.seekg(0, std::ios::beg);
  file.read(&buffer[0], fileSizeInByte);
	return buffer;
}

void writeFile(const std::string filePath, std::unordered_map<std::string, std::string>& texts)
{
  std::ofstream outFile(filePath);
  if (!outFile)
  {
		printf("\033[31mERR:\033[0m Failed to write file \"%s\"\n", filePath.data());
		exit(-1);
  }
  for (auto i : texts)
  {
    outFile << "const std::string ";
    outFile << i.first;
    outFile << " = \"";
    outFile << i.second, false;
    outFile << "\";\n";
  }

  outFile.close();
}
