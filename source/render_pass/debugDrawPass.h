#pragma once
#include "commonRenderPass.h"

class Renderer;
class Buffer;
class DebugDrawPass : public CommonRenderPass {
public:
	DebugDrawPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

	struct VertexA {
		glm::vec3 position;
		glm::vec4 color;
	};

private:
	nri::PipelineLayout *m_PipelineLayout = nullptr;
	nri::Pipeline *m_Pipeline = nullptr;
	std::shared_ptr<Buffer> m_GeometryBuffer;
	uint32_t m_IndexCount = 0;
	uint32_t m_NumMeshes = 32 * 1024;
	uint64_t m_indicesOffset = 0;
	std::vector<VertexA> m_positions;
	std::vector<uint32_t> m_indices;
};