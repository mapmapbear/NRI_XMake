#pragma once
#include "NRIDescs.h"
#include "NRIFramework.h"

struct Vertex {
	vec3 position;
	vec2 uv;
	vec3 normal;
	Vertex(vec3 pos, vec2 uv, vec3 nor) :
			position(pos), uv(uv), normal(nor) {}
};

struct ConstantBufferLayout {
	glm::mat4 modelMat;
	glm::mat4 viewMat;
	glm::mat4 projectMat;
};

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