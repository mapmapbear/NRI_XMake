#pragma once
#include "glm/mat4x4.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

class Buffer;
class Texture;
class Renderer;
struct aiMesh;
class SubMesh {
	friend class Mesh;

public:
	int GetMaterialID() { return m_materialID; }
	SubMesh() = default;
	// SubMesh(SubMesh &&) = default;
	SubMesh(const SubMesh &) = default;
	int m_materialID = -1;
	int m_indexCount = 0;
	int m_vertexCount = 0;

	std::shared_ptr<Buffer> m_vertexbuffer;
	std::shared_ptr<Buffer> m_indexbuffer;
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	glm::mat4 globalTransform;
};

class Material {
public:
	std::shared_ptr<Texture> m_BaseTexture;
	std::shared_ptr<Texture> m_NormalTexture;
	std::shared_ptr<Texture> m_MetallicTexture;
	bool IsTransparent = false;
};

class Mesh {
public:
	void LoadFromUSD(std::string &path, Renderer *renderer);
	int GetMeshCount() const { return (int)m_Meshes.size(); }
	SubMesh *GetMesh(const int index) const { return m_Meshes[index].get(); }
	const Material &GetMaterial(int materialId) const { return m_Materials[materialId]; }
	Mesh() = default;
	Mesh(Mesh &&) = default;
	Mesh(const Mesh &) = default;
	std::map<uint32_t, glm::mat4> results = {};

	// private:
public:
	std::unique_ptr<SubMesh> LoadMesh(aiMesh *pMesh, Renderer *renderer);

	std::vector<std::unique_ptr<SubMesh>> m_Meshes;
	std::vector<Material> m_Materials;
};