#pragma once
#include "commonRenderPass.h"



class Renderer;
class CommonMeshPass : public CommonRenderPass {
public:
	CommonMeshPass(Renderer *renderer, utils::Scene &scene);
	void Render(struct RenderInfo &info, Camera &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

private:
	nri::PipelineLayout *m_PipelineLayout = nullptr;
	nri::Pipeline *m_Pipeline = nullptr;
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

	nri::Descriptor *m_texture_albedo_view = nullptr;
	nri::Descriptor *m_texture_normal_view = nullptr;
	nri::Descriptor *m_texture_mr_view = nullptr;
	nri::Descriptor *m_texture_ao_view = nullptr;
	nri::Descriptor *m_texture_emissive_view = nullptr;

	// --------------------------------------------
	nri::Texture *m_CubemapTexture = nullptr;
	nri::Buffer *m_ConstantBuffer = nullptr;
	nri::Buffer *m_GeometryBuffer = {};
	nri::Buffer *m_MatrixStorageBuffer = nullptr;

	nri::Descriptor *m_CubemapTextureShaderResource = nullptr;
	nri::Descriptor *m_MatrixStorageBufferSRV = nullptr;
	nri::Descriptor *m_ConstantBufferView = nullptr;
	nri::Descriptor *m_Sampler = nullptr;

	nri::DescriptorSet *m_TextureDescriptorSet = nullptr;
	nri::DescriptorSet *m_ConstantBufferDescriptorSet = nullptr;
	uint64_t m_GeometryOffset = 0;
	uint32_t m_IndexCount = 0;
	uint32_t m_NumMeshes = 32 * 1024;

	std::vector<Vertex> m_positions;
	std::vector<uint32_t> m_indices;

	uint64_t m_indexDataAlignedTotalSize = 0;
	uint64_t m_vertexDataTotalSize = 0;
	std::vector<std::pair<uint64_t, uint64_t>> m_sceneMeshOffsets;

	utils::Scene &m_Scene;
};