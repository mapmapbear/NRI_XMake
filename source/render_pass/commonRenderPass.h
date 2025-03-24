#pragma once
#include "NRIDescs.h"
#include "NRIFramework.h"

class Renderer;
class CommonRenderPass {
public:
	CommonRenderPass(Renderer *renderer);
	virtual void Render(struct RenderInfo &info, Camera &camera) = 0;
    virtual void BuildPipeline() = 0;
    virtual void AllocGPUMemory() = 0;
    virtual void BindMemory() = 0;
protected:
	Renderer *m_renderer;
	NRIInterface *m_NRI;
	std::vector<nri::Memory *> m_MemoryAllocations;
};