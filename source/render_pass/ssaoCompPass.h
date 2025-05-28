#pragma once
#include "CommonRenderPass.h"

struct PushConstants {
	uint32_t texDepth;
	uint32_t texRotation;
	uint32_t texOut;
	uint32_t smpl;
	float zNear;
	float zFar;
	float radius;
	float attScale;
	float distScale;
};

class Texture;
class SSAOCompPass : public CommonRenderPass {
public:
	SSAOCompPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

private:
	nri::PipelineLayout *m_SSAOPipelineLayout = nullptr;
	nri::Pipeline *m_SSAOPipeline = nullptr;

	// nri::Texture *m_DepthTexture = nullptr;
	nri::Descriptor *m_DepthTextureShaderResource = nullptr;
	nri::Descriptor *m_Sampler = nullptr;
	// nri::Texture *m_NormalTexture = nullptr;
	// nri::Descriptor *m_NormalTextureShaderResource = nullptr;
	// nri::Texture *m_NoiseTexture = nullptr;
	// utils::Texture m_noiseTexture;
	// nri::Descriptor *m_NoiseTextureShaderResource = nullptr;
	// nri::Descriptor *m_SSAOTextureShaderResource = nullptr;
	// nri::Descriptor *m_Sampler = nullptr;
	nri::DescriptorSet *m_SSAOTextureDescriptorSet = nullptr;

	std::shared_ptr<Texture> m_SSAOTexture = nullptr;
	std::shared_ptr<Texture> m_RotationTexture = nullptr;
};
