#include "renderer.h"
#include "render_pass/skyRenderPass.h"
#include <memory>

Renderer::Renderer(NRIInterface &NRI, nri::Device *device) :
		m_Device(device), m_NRI(NRI) {
	NRI_ABORT_ON_FAILURE(NRI.GetQueue(*m_Device, nri::QueueType::GRAPHICS, 0, m_GraphicsQueue));
	NRI.SetDebugName(m_GraphicsQueue, "GraphicsQueue");

	NRI_ABORT_ON_FAILURE(NRI.GetQueue(*m_Device, nri::QueueType::COMPUTE, 0, m_ComputeQueue));
	NRI.SetDebugName(m_ComputeQueue, "ComputeQueue");

	nri::DescriptorPoolDesc descriptorPoolDesc = {};
	descriptorPoolDesc.descriptorSetMaxNum = BUFFERED_FRAME_MAX_NUM + 5;
	descriptorPoolDesc.constantBufferMaxNum = BUFFERED_FRAME_MAX_NUM;
	descriptorPoolDesc.storageBufferMaxNum = 2;
	descriptorPoolDesc.structuredBufferMaxNum = 2;
	descriptorPoolDesc.textureMaxNum = 20;
	descriptorPoolDesc.samplerMaxNum = 10;

	NRI_ABORT_ON_FAILURE(NRI.CreateDescriptorPool(*m_Device, descriptorPoolDesc,
			m_DescriptorPool));
}

void Renderer::OnStart() {
	skyPass = std::make_shared<SkyRenderPass>(this);
}

void Renderer::OnRender(RenderInfo &info) {
	skyPass->Render(info);
}
