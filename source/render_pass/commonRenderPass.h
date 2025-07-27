#pragma once
#include "NRIDescs.h"
#include "NRIFramework.h"

// struct Vertex {
// 	vec3 position;
// 	vec2 uv;
// 	vec3 normal;
// 	vec3 tangent;
// 	vec3 bitangent;
// 	Vertex(vec3 pos, vec2 uv, vec3 nor, vec3 tan, vec3 bitan) :
// 			position(pos), uv(uv), normal(nor), tangent(tan), bitangent(bitan) {}
// };

struct ConstantBufferLayout {
	glm::mat4 modelMat;
	glm::mat4 viewMat;
	glm::mat4 projectMat;
	glm::mat4 viewProjMat;
	glm::mat4 lightVP[4];
	glm::vec4 splitDepth;
	glm::vec4 cameraPosition;
};

class Renderer;
class CommonRenderPass {
public:
	CommonRenderPass(Renderer *renderer);
	virtual void Render(struct RenderInfo &info, Camera1 &camera) = 0;
	virtual void BuildPipeline() = 0;
	virtual void AllocGPUMemory() = 0;
	virtual void BindMemory() = 0;

protected:
	Renderer *m_renderer;
	NRIInterface *m_NRI;
	std::vector<nri::Memory *> m_MemoryAllocations;
};