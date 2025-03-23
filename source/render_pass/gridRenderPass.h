#pragma once
#include "NRIDescs.h"
#include "NRIFramework.h"


class Renderer;
class GridRenderPass {
public:
GridRenderPass(Renderer *renderer);
	void Render(struct RenderInfo& info, Camera& camera);

private:
	Renderer *m_renderer;
	NRIInterface *m_NRI;
	std::vector<nri::Memory *> m_MemoryAllocations;

	nri::PipelineLayout *m_GridPipelineLayout = nullptr;
	nri::Pipeline *m_GridPipeline = nullptr;
};