#pragma once
#include "commonRenderPass.h"

struct Vertex {
	vec3 position;
	vec2 uv;
	vec3 normal;
	Vertex(vec3 pos, vec2 uv, vec3 nor) :
			position(pos), uv(uv), normal(nor) {}
};

struct ConstantBufferLayout {
	glm::mat4 modelMat;
	glm::mat4 viewMat;
	glm::mat4 projectMat;
};

class Renderer;
class InstanceMeshPass : public CommonRenderPass {
public:
	InstanceMeshPass(Renderer *renderer);
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
	nri::Buffer *m_GeometryBuffer = nullptr;
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
};