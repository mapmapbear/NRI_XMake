#pragma once
#include "CommonRenderPass.h"
#include "NRIDescs.h"

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

struct BlurPushConstants {
	uint32_t texDepth;
    uint32_t texIn;
    uint32_t texOut;
    uint32_t smpl;
    float depthThreshold;
	float isHorizontal;
};

class Texture;
class SSAOCompPass : public CommonRenderPass {
public:
	SSAOCompPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera1 &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;
	std::shared_ptr<Texture> m_SSAOTexture = nullptr;

private:
	nri::PipelineLayout *m_SSAOPipelineLayout = nullptr;
	nri::Pipeline *m_SSAOPipeline = nullptr;

	nri::PipelineLayout *m_BlurPipelineLayoutX = nullptr;
	nri::PipelineLayout *m_BlurPipelineLayoutY = nullptr;
	// nri::PipelineLayout *m_BlurPipelineLayout = nullptr;
	nri::Pipeline *m_BlurPipelineX = nullptr;
	nri::Pipeline *m_BlurPipelineY = nullptr;
	

	// nri::Texture *m_DepthTexture = nullptr;
	nri::Descriptor *m_DepthTextureShaderResource = nullptr;
	nri::Descriptor *m_SSAOOutSRV = nullptr;
	nri::Descriptor *m_Sampler = nullptr;
	// nri::Texture *m_NormalTexture = nullptr;
	// nri::Descriptor *m_NormalTextureShaderResource = nullptr;
	// nri::Texture *m_NoiseTexture = nullptr;
	// utils::Texture m_noiseTexture;
	// nri::Descriptor *m_NoiseTextureShaderResource = nullptr;
	// nri::Descriptor *m_SSAOTextureShaderResource = nullptr;
	// nri::Descriptor *m_Sampler = nullptr;
	nri::DescriptorSet *m_SSAOTextureDescriptorSet = nullptr;

	std::shared_ptr<Texture> m_RotationTexture = nullptr;
};
