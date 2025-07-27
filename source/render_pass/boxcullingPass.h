#pragma once
#include "CommonRenderPass.h"
class Buffer;
class BoxCullingPass : public CommonRenderPass {
public:
	BoxCullingPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera1 &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

private:
	nri::PipelineLayout *m_PipelineLayout = nullptr;
	nri::Pipeline *m_Pipeline = nullptr;
	nri::DescriptorSet *m_DescriptorSet = nullptr;

	std::vector<utils::Vertex> m_positions;
	std::vector<uint32_t> m_indices;
	uint32_t m_IndexCount = 0;
	uint64_t m_GeometryOffset = 0;

	std::shared_ptr<Buffer> m_GeometryBuffer;

	std::shared_ptr<Buffer> m_positionBuffer = nullptr;
	std::shared_ptr<Buffer> m_vertexPosBuffer = nullptr;
	std::shared_ptr<Buffer> m_indirectBuffer = nullptr;
};