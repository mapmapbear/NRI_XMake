#pragma once
#include "CommonRenderPass.h"
#include <array>
class Buffer;

class GPUCullingPass : public CommonRenderPass {
public:
	GPUCullingPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

	struct CullData {
		glm::vec3 center;
		float radians;
	};

	struct PushConstants {
		glm::mat4 viewMat;
		glm::vec4 cameraArgs;
		std::array<glm::vec4, 4> frustum;
		uint32_t totalObjectCount;
	};

public:
	std::shared_ptr<Buffer> m_CullGPUSceneObjectsBuffer = nullptr;

private:
	nri::PipelineLayout *m_CullingPipelineLayout = nullptr;
	nri::Pipeline *m_CullingPipeline = nullptr;
	nri::DescriptorSet *m_CullingDescriptorSet = nullptr;

	std::shared_ptr<Buffer> m_CullDataBuffer = nullptr;
	std::shared_ptr<Buffer> m_GPUSceneObjectsBuffer = nullptr;
	std::shared_ptr<Buffer> m_VisibleObjectCounterBuffer = nullptr;

};
