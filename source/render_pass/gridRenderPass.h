#pragma once
#include "commonRenderPass.h"

class Renderer;
class GridRenderPass : public CommonRenderPass {
public:
	GridRenderPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera1 &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

private:
	nri::PipelineLayout *m_GridPipelineLayout = nullptr;
	nri::Pipeline *m_GridPipeline = nullptr;
};