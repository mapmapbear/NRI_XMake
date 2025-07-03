#pragma once
#include "glm/mat4x4.hpp"
#include <meshoptimizer.h>
#include <map>
#include <memory>
#include <string>
#include <taskflow/taskflow.hpp>
#include <vector>

class Buffer;
class Texture;
class Renderer;
struct aiMesh;
struct aiScene;

struct DrawArgsOffset {
	uint32_t indexNum;
	uint32_t vertexNum;
	uint32_t indexOffset;
	uint32_t vertexOffset;
};

struct DrawArgsBase {
	uint32_t indexNum;
	uint32_t vertexNum;
	uint32_t baseIndex;
	uint32_t baseVertex;
};

struct DrawArgs {
	DrawArgsOffset offset;
	DrawArgsBase base;
};

class SubMesh {
	friend class Mesh;

public:
	int GetMaterialID() const { return m_materialID; }
	SubMesh() = default;
	// SubMesh(SubMesh &&) = default;
	SubMesh(const SubMesh &) = default;
	glm::mat4 GetTransform() const { return globalTransform; }
	int m_materialID = -1;
	int m_indexCount = 0;
	int m_vertexCount = 0;
	std::string name;
	std::shared_ptr<Buffer> m_vertexbuffer;
	std::shared_ptr<Buffer> m_indexbuffer;
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	glm::mat4 globalTransform;
	std::pair<glm::vec3, glm::vec3> aabb = { glm::vec3(0.0f), glm::vec3(0.0f) };
	std::pair<glm::vec3, glm::vec3> aabb2 = { glm::vec3(0.0f), glm::vec3(0.0f) };

	struct MeshLet {
		std::vector<meshopt_Meshlet> m_meshlets;
		std::vector<meshopt_Bounds> m_bounds;
		std::vector<std::vector<uint32_t>> m_clusters;
		std::vector<DrawArgs> m_drawArgs;
		uint32_t m_cluster_total_size = 0;
	};
	std::vector<MeshLet> m_meshlet;
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
	void LoadFromUSD(std::string &path, Renderer *renderer, bool meshlet = false);
	int GetMeshCount() const { return (int)m_Meshes.size(); }
	SubMesh *GetMesh(const int index) const { return m_Meshes[index].get(); }
	const Material &GetMaterial(int materialId) { return m_Materials[materialId]; }
	Mesh() = default;
	Mesh(Mesh &&) = default;
	Mesh(const Mesh &) = default;
	std::map<uint32_t, glm::mat4> results = {};

	// private:
public:
	std::unique_ptr<SubMesh> LoadMesh(aiMesh *pMesh, Renderer *renderer, bool meshlet = false);
	std::unique_ptr<SubMesh> LoadGPUMesh(const aiScene *scene, uint32_t numMeshes, Renderer *renderer, bool meshlet);

	std::vector<std::unique_ptr<SubMesh>> m_Meshes;
	std::unique_ptr<SubMesh> m_GPUMesh;
	std::vector<Material> m_Materials;
	std::vector<DrawArgs> m_drawArgs;
	std::vector<meshopt_Meshlet> m_meshlets;

	tf::Executor executor;
	tf::Taskflow taskflow;
};