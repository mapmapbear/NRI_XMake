#pragma once
#include "NRIDescs.h"
#include "NRIFramework.h"
#include <future>
#include <memory>

struct RenderInfo {
	nri::AttachmentsDesc &desc;
	nri::CommandBuffer &cmdBuffer;
};

class SkyRenderPass;
class GridRenderPass;
class InstanceMeshPass;
class PresentPass;
class CommonMeshPass;
class Renderer {
public:
	Renderer(NRIInterface &NRI, nri::Device *device);
	nri::Device *GetRenderDevice() { return m_Device; }
	NRIInterface &GetNRI() { return m_NRI; }
	nri::DescriptorPool &GetDescriptorPool() { return *m_DescriptorPool; }
	nri::Queue &GetRenderQueue() { return *m_GraphicsQueue; }
	nri::DescriptorSet *GetGloablDescriptorSet() { return m_GloablFrameDescriptorSet; }

	void OnStart(nri::DescriptorSet *globalSet);
	void OnUpdate();
	void OnPreRender();
	void OnRender(RenderInfo &info, Camera &camera);
	void OnPresent(RenderInfo &info);
	void OnPostRender();
	void InitPresentPass(nri::Texture *colorRT, nri::SwapChain *swawpchain);

	utils::Texture& GetDefaultBlackTex() { return defaultBlackTex; }
	utils::Texture& GetDefaultWhiteTex() { return deaflutWhiteTex; }
	utils::Texture& GetDefaultNormalTex() { return defaultNormalTex; }

public:
	int testIndex = 0;
	int texViewOffset = 0;
	glm::vec4 testVec = glm::vec4{0.0};
	void SetTestIndex(int index) { testIndex = index; }
private:
	nri::Device *m_Device = nullptr;
	NRIInterface &m_NRI;
	nri::DescriptorPool *m_DescriptorPool = nullptr;
	nri::Queue *m_GraphicsQueue = nullptr;
	nri::Queue *m_ComputeQueue = nullptr;

	nri::DescriptorSet *m_GloablFrameDescriptorSet = nullptr;
	utils::Scene m_Scene;
public:
	utils::Texture defaultBlackTex;
	utils::Texture deaflutWhiteTex;
	utils::Texture defaultNormalTex;
	utils::Texture diffuseIrradianceTex;
	utils::Texture specularIrradianceTex;
	utils::Texture BRDFTex;

	nri::Texture *m_DiffuseIrradianceTex = nullptr;
	nri::Texture *m_SpecularIrradianceTex = nullptr;
	nri::Texture *m_BRDFTex = nullptr;

	std::vector<nri::Memory *> m_MemoryAllocations;
private:
	std::shared_ptr<SkyRenderPass> skyPass = nullptr;
	std::shared_ptr<GridRenderPass> gridPass = nullptr;
	std::shared_ptr<InstanceMeshPass> meshPass = nullptr;
	std::shared_ptr<CommonMeshPass> simplePass = nullptr;
	std::shared_ptr<PresentPass> presentPass = nullptr;
};