#pragma once
#include "commonRenderPass.h"

class Renderer;
class Buffer;
class DebugDrawPass : public CommonRenderPass {
public:
	DebugDrawPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera1 &camera) override;
	void BuildPipeline() override;
	void AllocGPUMemory() override;
	void BindMemory() override;
	void DrawBox(const glm::vec3 &center, const glm::vec3 &extent, const glm::vec4 &color);
	void DrawFrustum(const glm::vec3 &center);
	void DrawSphere(const glm::vec3 &center, float radius, const glm::vec4 &color);
	void GenerateBoundingSphere(float radius, int segments);
	void GenerateFrustum(float near, float far, float fov, float aspect);
	void GenerateBoxBuffer();

	struct VertexA {
		glm::vec3 position;
		glm::vec4 color;
	};

	struct BoxMeshData {
		glm::mat4 worldMat;
	};

	struct ConstantBlock {
	glm::mat4 modelMat;
	glm::vec4 camPos;
	glm::vec4 testVec;
	uint32_t index[4];
};


private:
	nri::PipelineLayout *m_PipelineLayout = nullptr;
	nri::DescriptorSet *m_DescriptorSet = nullptr;
	nri::Buffer *m_ConstantBuffer = nullptr;
	nri::DescriptorSet *m_ConstantBufferDescriptorSet = nullptr;
	nri::Descriptor *m_ConstantBufferView = nullptr;

	// Box
	std::vector<VertexA> m_positions;
	std::vector<uint32_t> m_indices;
	uint64_t m_indicesOffset = 0;
	nri::Pipeline *m_Pipeline = nullptr;
	std::shared_ptr<Buffer> m_GeometryBuffer;

	// Sphere
	std::vector<VertexA> m_positions_sphere;
	std::vector<uint32_t> m_indices_sphere;
	uint64_t m_indicesOffset_sphere = 0;
	nri::Pipeline *m_Pipeline_sphere = nullptr;
	std::shared_ptr<Buffer> m_GeometryBuffer_sphere;

	//Frustum
	std::vector<VertexA> m_positions_frustum;
	std::vector<uint32_t> m_indices_frustum;
	uint64_t m_indicesOffset_frustum = 0;
	nri::Pipeline *m_Pipeline_frustum = nullptr;
	std::shared_ptr<Buffer> m_GeometryBuffer_frustum;

	// Rect
	nri::Pipeline *m_Pipeline_rect = nullptr;

	// Global World Matrices
	std::vector<glm::mat4> boxWorldMats;
	std::shared_ptr<Buffer> m_boxDataBuffer;
	uint32_t m_box_count = 0;
	uint32_t m_sphere_count = 0;
	uint32_t m_frustum_count = 0;

};