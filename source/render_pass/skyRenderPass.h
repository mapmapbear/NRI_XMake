#pragma once
#include "CommonRenderPass.h"

class SkyRenderPass : public CommonRenderPass {
public:
	SkyRenderPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

private:
	nri::PipelineLayout *m_SkyPipelineLayout = nullptr;
	nri::Pipeline *m_SkyPipeline = nullptr;
	nri::Texture *m_HDRTexture = nullptr;
	utils::Texture texture;
	tinyddsloader::DDSFile *m_HDRTexture_DDS = nullptr;
	nri::Descriptor *m_HDRTextureShaderResource = nullptr;
	nri::Descriptor *m_Sampler = nullptr;
	nri::DescriptorSet *m_SkyTextureDescriptorSet = nullptr;
};