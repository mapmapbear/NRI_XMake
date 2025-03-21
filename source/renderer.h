#pragma once
#include "NRIDescs.h"
#include "NRIFramework.h"
#include <memory>


struct RenderInfo
{
	nri::AttachmentsDesc& desc;
	nri::CommandBuffer& cmdBuffer;
};

class SkyRenderPass;
class Renderer {
public:
	Renderer(NRIInterface &NRI, nri::Device *device);
	nri::Device *GetRenderDevice() { return m_Device; }
	NRIInterface &GetNRI() { return m_NRI; }
	nri::DescriptorPool &GetDescriptorPool() { return *m_DescriptorPool; }
	nri::Queue &GetRenderQueue() { return *m_GraphicsQueue; }

	void OnStart();
	void OnUpdate();
	void OnPreRender();
	void OnRender(RenderInfo& info);
	void OnPostRender();

private:
	nri::Device *m_Device = nullptr;
	NRIInterface &m_NRI;
	nri::DescriptorPool *m_DescriptorPool = nullptr;
	nri::Queue *m_GraphicsQueue = nullptr;
	nri::Queue *m_ComputeQueue = nullptr;

private:
	std::shared_ptr<SkyRenderPass> skyPass = nullptr;
};