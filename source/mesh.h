#pragma once
#include <memory>
#include <string>
#include <vector>

class Buffer;
class Texture;
class Renderer;
class aiMesh;
class SubMesh {
	friend class Mesh;

public:
	int GetMaterialID();

private:
	int m_materialID = -1;
	int m_indexCount = 0;
	int m_vertexCount = 0;

	std::unique_ptr<Buffer> m_vertexbuffer;
	std::unique_ptr<Buffer> m_indexbuffer;
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
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

private:
	std::unique_ptr<SubMesh> LoadMesh(aiMesh *pMesh, Renderer *renderer);

	std::vector<std::unique_ptr<SubMesh>> m_Meshes;
	std::vector<Material> m_Materials;
};