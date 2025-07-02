#pragma once
#include "Camera.h"
#include "NRIDescs.h"
#include "NRIFramework.h"
#include "mesh.h"
#include "render_pass/debugDrawPass.h"
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

struct RenderInfo {
	nri::AttachmentsDesc &desc;
	nri::CommandBuffer &cmdBuffer;
};

struct RenderSetting {
	bool IndirectDrawState = true;
	bool DebugBoxState = false;
	bool DebugSSAOState = false;
};

class SkyRenderPass;
class GridRenderPass;
class InstanceMeshPass;
class SSAOCompPass;
class DebugDrawPass;
class PresentPass;
class BoxCullingPass;
class GPUCullingPass;
class CommonMeshPass;
class Buffer;
class Texture;
class Renderer {
public:
	Renderer(NRIInterface &NRI, nri::Device *device, Camera &camera);
	nri::Device *GetRenderDevice() { return m_Device; }
	NRIInterface &GetNRI() { return m_NRI; }
	nri::DescriptorPool &GetDescriptorPool() { return *m_DescriptorPool; }
	nri::Queue &GetRenderQueue() { return *m_GraphicsQueue; }
	nri::DescriptorSet *GetGloablDescriptorSet() { return m_GloablFrameDescriptorSet; }

	void OnStart(nri::DescriptorSet *globalSet, nri::Texture *colorTex, nri::Texture *depthTex);
	void OnUpdate(float deltaTime);
	void OnPreRender();
	void OnRenderDepth(RenderInfo &info, Camera &camera);
	void OnRender(RenderInfo &info, Camera &camera);
	void OnPresent(RenderInfo &info);
	void OnPostRender();
	void InitPresentPass(nri::Texture *colorRT, nri::SwapChain *swawpchain);
	void UploadSceneData();

	void BindCamera(Camera &camera) { m_Camera = camera; }

	utils::Texture &GetDefaultBlackTex() { return defaultBlackTex; }
	utils::Texture &GetDefaultWhiteTex() { return defaultWhiteTex; }
	utils::Texture &GetDefaultNormalTex() { return defaultNormalTex; }

	std::shared_ptr<Texture> GetDefaultBlackTexPtr() { return m_DefaultBlackTex; }
	std::shared_ptr<Texture> GetDefaultWhiteTexPtr() { return m_DefaultWhiteTex; }
	std::shared_ptr<Texture> GetDefaultNormalTexPtr() { return m_DefaultNormalTex; }

private:
	void RandomLights();
	glm::mat4 computeLightSpaceMatrix(float yaw, float pitch, float roll);

public:
	int testIndex = 0;
	int texViewOffset = 0;
	glm::vec4 testVec = glm::vec4{ 0.0 };
	RenderSetting m_config;
	float testMaterial = 0.0;
	float testRoughness = 0.0;
	void SetTestIndex(int index) { testIndex = index; }
	std::unordered_map<std::shared_ptr<Texture>, std::shared_ptr<utils::Texture>> uploadTextureMap;
	std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<utils::MeshData>> uploadIndexBufferMap;
	std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<utils::MeshData>> uploadShadowIndexBufferMap;
	Camera& m_Camera;

private:
	nri::Device *m_Device = nullptr;
	NRIInterface &m_NRI;
	nri::DescriptorPool *m_DescriptorPool = nullptr;
	nri::Queue *m_GraphicsQueue = nullptr;
	nri::Queue *m_ComputeQueue = nullptr;

	nri::DescriptorSet *m_GloablFrameDescriptorSet = nullptr;

public:
	utils::Scene m_Scene;
	std::pair<glm::vec3, glm::vec3> m_SceneAABB;
	std::pair<uint32_t, uint32_t> m_OutputResolution = { 1920, 1080 };
	bool m_firstFrame = true;
	glm::mat4 m_lightVP = glm::mat4(1.0);
	glm::vec3 m_lightPos = glm::vec3(0.2, 100.0, 0.2);

	struct RenderNode {
		const SubMesh *mesh;
		const SubMesh *meshGPU;
		// std::pair<uint64_t, uint64_t> meshOffsets;
		nri::DrawIndexedDesc drawArgs;
		std::vector<nri::DrawIndexedDesc> clusterDrawArgs;
		const Material *material;
		glm::mat4 globalTransform;
	};

	std::vector<RenderNode> m_OpaqueRenderNodes;
	std::vector<RenderNode> m_ClusterRenderNodes;
	std::vector<RenderNode> m_TransparentRenderNodes;

public:
	utils::Texture defaultBlackTex;
	utils::Texture defaultWhiteTex;
	utils::Texture defaultNormalTex;
	utils::Texture diffuseIrradianceTex;
	utils::Texture specularIrradianceTex;
	utils::Texture BRDFTex;

	nri::Texture *m_DiffuseIrradianceTex = nullptr;
	nri::Texture *m_SpecularIrradianceTex = nullptr;
	nri::Texture *m_BRDFTex = nullptr;

	nri::Texture *m_ColorTex = nullptr;
	nri::Texture *m_DepthTex = nullptr;

	std::shared_ptr<Texture> m_DefaultBlackTex = nullptr;
	std::shared_ptr<Texture> m_DefaultWhiteTex = nullptr;
	std::shared_ptr<Texture> m_DefaultNormalTex = nullptr;
	std::shared_ptr<Texture> m_ShadowMap = nullptr;

	std::vector<nri::Memory *> m_MemoryAllocations;

public:
	std::shared_ptr<GPUCullingPass> gpuCullingPass = nullptr;

private:
	std::shared_ptr<SkyRenderPass> skyPass = nullptr;
	std::shared_ptr<GridRenderPass> gridPass = nullptr;
	std::shared_ptr<InstanceMeshPass> meshPass = nullptr;
	std::shared_ptr<CommonMeshPass> simplePass = nullptr;
	std::shared_ptr<SSAOCompPass> ssaoCompPass = nullptr;
	std::shared_ptr<DebugDrawPass> debugdrawPass = nullptr;
	std::shared_ptr<BoxCullingPass> boxCullingPass = nullptr;
	std::shared_ptr<PresentPass> presentPass = nullptr;
};