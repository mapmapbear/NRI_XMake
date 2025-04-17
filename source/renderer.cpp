#include "renderer.h"
#include "Camera.h"
#include "render_pass/commonMeshPass.h"
#include "render_pass/gridRenderPass.h"
#include "render_pass/instanceMeshPass.h"
#include "render_pass/presentPass.h"
#include "render_pass/skyRenderPass.h"
#include <memory>

Renderer::Renderer(NRIInterface &NRI, nri::Device *device) :
		m_Device(device), m_NRI(NRI) {
	NRI_ABORT_ON_FAILURE(NRI.GetQueue(*m_Device, nri::QueueType::GRAPHICS, 0, m_GraphicsQueue));
	NRI.SetDebugName(m_GraphicsQueue, "GraphicsQueue");

	NRI_ABORT_ON_FAILURE(NRI.GetQueue(*m_Device, nri::QueueType::COMPUTE, 0, m_ComputeQueue));
	NRI.SetDebugName(m_ComputeQueue, "ComputeQueue");

	nri::DescriptorPoolDesc descriptorPoolDesc = {};
	descriptorPoolDesc.descriptorSetMaxNum = BUFFERED_FRAME_MAX_NUM + 6;
	descriptorPoolDesc.constantBufferMaxNum = BUFFERED_FRAME_MAX_NUM;
	descriptorPoolDesc.storageBufferMaxNum = 2;
	descriptorPoolDesc.structuredBufferMaxNum = 2;
	descriptorPoolDesc.textureMaxNum = 50;
	descriptorPoolDesc.samplerMaxNum = 10;

	NRI_ABORT_ON_FAILURE(NRI.CreateDescriptorPool(*m_Device, descriptorPoolDesc,
			m_DescriptorPool));
	NRI.SetDebugName(m_DescriptorPool, "m_DescriptorPool");

	std::string sceneFile = utils::GetFullPath("Camera/Camera.gltf", utils::DataFolder::ROOT);
	sceneFile = utils::GetFullPath("test.glb", utils::DataFolder::ROOT);
	NRI_ABORT_ON_FALSE(utils::LoadScene(sceneFile, m_Scene, false));
}

void Renderer::OnStart(nri::DescriptorSet *globalSet) {
	skyPass = std::make_shared<SkyRenderPass>(this);
	gridPass = std::make_shared<GridRenderPass>(this);
	meshPass = std::make_shared<InstanceMeshPass>(this);
	simplePass = std::make_shared<CommonMeshPass>(this, m_Scene);
}
void Renderer::InitPresentPass(nri::Texture *colorRT, nri::SwapChain *swawpchain) {
	presentPass = std::make_shared<PresentPass>(this, colorRT, swawpchain);
}
void Renderer::OnRender(RenderInfo &info, Camera &camera) {
	skyPass->Render(info, camera);
	gridPass->Render(info, camera);
	meshPass->Render(info, camera);
	simplePass->Render(info, camera);
}

void Renderer::OnPresent(RenderInfo &info) {
	Camera dummyCamera; // Create a dummy Camera object
	presentPass->Render(info, dummyCamera);
}
