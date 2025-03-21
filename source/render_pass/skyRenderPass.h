#pragma once
#include "NRIDescs.h"
#include "NRIFramework.h"


class Renderer;
class SkyRenderPass {
public:
	SkyRenderPass(Renderer *renderer);
	void Render(struct RenderInfo& info);

private:
	Renderer *m_renderer;
	NRIInterface *m_NRI;
	std::vector<nri::Memory *> m_MemoryAllocations;

	nri::PipelineLayout *m_SkyPipelineLayout = nullptr;
	nri::Pipeline *m_SkyPipeline = nullptr;
	nri::Texture *m_HDRTexture = nullptr;
	nri::Descriptor *m_HDRTextureShaderResource = nullptr;
	nri::Descriptor *m_Sampler = nullptr;
	nri::DescriptorSet *m_SkyTextureDescriptorSet = nullptr;
};