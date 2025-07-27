#pragma once
#include "commonRenderPass.h"
#include <vector>

class PresentPass : public CommonRenderPass {
public:
	PresentPass(Renderer *renderer, nri::Texture *colorRT, nri::SwapChain *swapchain);
	void Render(struct RenderInfo &info, Camera1 &camera) override;
	// void Render(struct RenderInfo &src_info, struct RenderInfo &dst_info);

	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

private:
	nri::PipelineLayout *m_graphicsPipelineLayout;
	nri::Pipeline *m_graphicsPipeline;
	nri::DescriptorSet *m_presentTextureDescriptorSet = nullptr;
	nri::Descriptor *m_ColorRTShaderResource = nullptr;
	nri::Texture *m_colorRT = nullptr;
	nri::Descriptor *m_Sampler = nullptr;
};