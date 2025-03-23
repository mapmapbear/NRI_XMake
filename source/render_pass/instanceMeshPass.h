#pragma once
#include "NRIDescs.h"
#include "NRIFramework.h"
#include <cstdint>

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
class InstanceMeshPass {
public:
	InstanceMeshPass(Renderer *renderer);
	void Render(struct RenderInfo &info, Camera &camera);

private:
	Renderer *m_renderer;
	NRIInterface *m_NRI;
	std::vector<nri::Memory *> m_MemoryAllocations;

	nri::PipelineLayout *m_PipelineLayout = nullptr;
	nri::Pipeline *m_Pipeline = nullptr;
	nri::Texture *m_Texture = nullptr;
	nri::Texture *m_CubemapTexture = nullptr;
	nri::Buffer *m_ConstantBuffer = nullptr;
	nri::Buffer *m_GeometryBuffer = nullptr;
	nri::Descriptor *m_TextureShaderResource = nullptr;
	nri::Descriptor *m_CubemapTextureShaderResource = nullptr;

	uint64_t m_GeometryOffset = 0;
	uint64_t m_IndexCount = 0;
};