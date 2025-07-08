#pragma once
#include "NRIDescs.h"
#include "commonRenderPass.h"
#include <memory>
#include <unordered_map>
#include <vector>

class Mesh;
class Renderer;
class Texture;
class Buffer;

struct CullData 
{
	glm::vec3 center;
	float radians;
};

class CommonMeshPass : public CommonRenderPass {
	friend class DebugDrawPass;

public:
	CommonMeshPass(Renderer *renderer, utils::Scene &scene, std::shared_ptr<Mesh> &rootMesh);
	void Render(struct RenderInfo &info, Camera &camera) override;
	void RenderDepth(struct RenderInfo &info, Camera &camera);
	void RenderShadow(struct RenderInfo &info, Camera &camera);
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

public:
	int testIndex = 0;
	void SetTestIndex(int index) { testIndex = index; }
	void GetMeshNode(utils::NodeData &node) {
		if (!node.meshIndices.empty()) {
			m_meshNodes.push_back(node);
		} else {
			for (auto child : node.children) {
				GetMeshNode(child);
			}
		}
	}

private:
	nri::PipelineLayout *m_PipelineLayout = nullptr;
	nri::Pipeline *m_Pipeline = nullptr;

	nri::PipelineLayout *m_DepthPipelineLayout = nullptr;
	nri::Pipeline *m_DepthPipeline = nullptr;

	nri::PipelineLayout *m_ShadowPipelineLayout = nullptr;
	nri::Pipeline *m_ShadowPipeline = nullptr;
	// ------------------------------------
	//             Material Data
	nri::Texture *m_texture_albedo = nullptr;
	nri::Texture *m_texture_normal = nullptr;
	nri::Texture *m_texture_mr = nullptr;
	nri::Texture *m_texture_ao = nullptr;
	nri::Texture *m_texture_emissive = nullptr;

	utils::Texture m_texture_albedo_data;
	utils::Texture m_texture_normal_data;
	utils::Texture m_texture_mr_data;
	utils::Texture m_texture_ao_data;
	utils::Texture m_texture_emissive_data;

	std::vector<utils::Texture> m_texureDatas;
	std::vector<nri::Texture *> m_textures;
	std::vector<nri::Descriptor *> m_textureViews;

	struct MaterialIndexBlock {
		uint32_t textureBase = 0;
		uint32_t textureNormal = 0;
		uint32_t textureMetallic = 0;
		uint32_t textureIndex3 = 0;
	};

	std::vector<MaterialIndexBlock> m_materialIndexBlocks;

	nri::Descriptor *m_texture_albedo_view = nullptr;
	nri::Descriptor *m_texture_normal_view = nullptr;
	nri::Descriptor *m_texture_mr_view = nullptr;
	nri::Descriptor *m_texture_ao_view = nullptr;
	nri::Descriptor *m_texture_emissive_view = nullptr;

	// --------------------------------------------
	nri::Texture *m_CubemapTexture = nullptr;
	nri::Buffer *m_ConstantBuffer = nullptr;
	nri::Buffer *m_GeometryBuffer = {};
	// std::shared_ptr<Buffer> m_GeometryBuffer1 = nullptr;
	nri::Buffer *m_MatrixStorageBuffer = nullptr;

	nri::Descriptor *m_CubemapTextureShaderResource = nullptr;
	nri::Descriptor *m_MatrixStorageBufferSRV = nullptr;
	nri::Descriptor *m_ConstantBufferView = nullptr;
	nri::Descriptor *m_Sampler = nullptr;
	nri::Descriptor *m_SamplerShadow = nullptr;

	nri::DescriptorSet *m_TextureDescriptorSet = nullptr;
	nri::DescriptorSet *m_ConstantBufferDescriptorSet = nullptr;
	uint64_t m_GeometryOffset = 0;
	uint32_t m_IndexCount = 0;
	uint32_t m_NumMeshes = 32 * 1024;

	std::vector<std::vector<utils::Vertex>> m_positions;
	std::vector<uint32_t> m_indices;

	uint64_t m_indexDataAlignedTotalSize = 0;
	uint64_t m_vertexDataTotalSize = 0;
	std::vector<std::pair<uint64_t, uint64_t>> m_sceneMeshOffsets;
	std::vector<utils::NodeData> m_meshNodes = {};
	utils::Scene &m_Scene;
	std::shared_ptr<Mesh> m_rootMesh;
	std::unordered_set<std::shared_ptr<Texture>> m_matTexSet;
	std::shared_ptr<Mesh> m_mesh;

	std::shared_ptr<Buffer> m_indirectBuffer = nullptr;
	std::shared_ptr<Buffer> m_worldMatBuffer = nullptr;
	std::shared_ptr<Buffer> m_sphereCullBuffer = nullptr;

	glm::mat4 m_lightVP = glm::mat4(1.0);
	uint32_t m_brdfTexIndex = 0;
};