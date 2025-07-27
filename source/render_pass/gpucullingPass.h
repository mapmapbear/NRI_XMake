#pragma once
#include "CommonRenderPass.h"
#include <array>
#include <memory>

class Buffer;
class Texture;
class GPUCullingPass : public CommonRenderPass {
public:
	GPUCullingPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera1 &camera) override;
	void RenderPost(struct RenderInfo &info, Camera1 &camera);
	void RenderHiZ(struct RenderInfo &info);
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;

	struct CullData {
		glm::vec3 center;
		glm::vec3 extents;
	};

	struct PushConstants {
		glm::mat4 viewMat;
		glm::vec4 cameraArgs;
		std::array<glm::vec4, 4> frustum;
		std::array<uint32_t, 4> totalObjectCount;
	};

	struct HiZPushConstants {
		float2 DimensionsInv;
		uint texDepth;
		uint texHiZ;
		uint sampleIndex;
	};
	

public:
	std::shared_ptr<Buffer> m_CullGPUSceneObjectsBuffer = nullptr;
	std::shared_ptr<Buffer> m_VisibleObjectCounterBuffer = nullptr;

private:
	nri::PipelineLayout *m_CullingPipelineLayout = nullptr;
	nri::Pipeline *m_CullingPipeline = nullptr;
	nri::Pipeline *m_CullingPipeline2 = nullptr;
	nri::DescriptorSet *m_CullingDescriptorSet = nullptr;
	nri::DescriptorSet *m_CullingDescriptorConstantBufferSet = nullptr;

	nri::PipelineLayout *m_HiZPipelineLayout = nullptr;
	nri::Pipeline *m_HiZPipeline = nullptr;
	nri::DescriptorSet *m_HiZDescriptorSet = nullptr;
	nri::Descriptor *m_DepthTextureSRV = nullptr;
	nri::Descriptor *m_HiZTextureSRV = nullptr;
	nri::Descriptor *m_PointSampler = nullptr;
	std::shared_ptr<Texture> m_HiZTexture = nullptr;

	std::shared_ptr<Buffer> m_CullDataBuffer = nullptr;
	std::shared_ptr<Buffer> m_GPUSceneObjectsBuffer = nullptr;
	std::shared_ptr<Buffer> m_VisibleFlagsBuffer = nullptr;

	nri::Buffer *m_ConstantBuffer = nullptr;
	nri::Descriptor *m_ConstantBufferView = nullptr;

};
