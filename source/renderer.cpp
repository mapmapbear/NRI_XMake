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

	// Init Defalut Resource
	utils::LoadTexture(utils::GetFullPath("black.png", utils::DataFolder::TEXTURES), defaultBlackTex);
	utils::LoadTexture(utils::GetFullPath("white.png", utils::DataFolder::TEXTURES), deaflutWhiteTex);
	utils::LoadTexture(utils::GetFullPath("normal.png", utils::DataFolder::TEXTURES), defaultNormalTex);

	std::string sceneFile = utils::GetFullPath("Camera/Camera.gltf", utils::DataFolder::ROOT);
	// sceneFile = utils::GetFullPath("test.glb", utils::DataFolder::ROOT);
	NRI_ABORT_ON_FALSE(utils::LoadScene(sceneFile, m_Scene, false));

	std::string diffuseIrrTex = "data/Textures/diffuseIrradiance.dds";
	if (!utils::LoadTexture(diffuseIrrTex, diffuseIrradianceTex, true)) {
		printf("Can not found this texture %s", diffuseIrrTex.c_str());
	}

	std::string specularIrrTex = "data/Textures/specularIrradiance.dds";
	if (!utils::LoadTexture(diffuseIrrTex, specularIrradianceTex, true)) {
		printf("Can not found this texture %s", specularIrrTex.c_str());
	}

	
}

void Renderer::OnStart(nri::DescriptorSet *globalSet) {
	auto NRI = m_NRI;

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = diffuseIrradianceTex.GetFormat();
		textureDesc.width = diffuseIrradianceTex.GetWidth();
		textureDesc.height = diffuseIrradianceTex.GetHeight();
		textureDesc.mipNum = diffuseIrradianceTex.GetMipNum();
		textureDesc.layerNum = diffuseIrradianceTex.GetArraySize();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_Device, textureDesc, m_DiffuseIrradianceTex));
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE; 
		textureDesc.format = specularIrradianceTex.GetFormat();
		textureDesc.width = specularIrradianceTex.GetWidth();
		textureDesc.height = specularIrradianceTex.GetHeight();
		textureDesc.mipNum = specularIrradianceTex.GetMipNum();
		textureDesc.layerNum = specularIrradianceTex.GetArraySize();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_Device, textureDesc, m_SpecularIrradianceTex));
	}

	std::vector<nri::Texture *> textureArray = { m_DiffuseIrradianceTex, m_SpecularIrradianceTex };
	nri::ResourceGroupDesc resourceGroupDesc = {};
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.textureNum = textureArray.size();
	resourceGroupDesc.textures = textureArray.data();

	m_MemoryAllocations.resize(
			NRI.CalculateAllocationNumber(*m_Device, resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_Device, resourceGroupDesc, m_MemoryAllocations.data()));

	NRI.SetDebugName(m_DiffuseIrradianceTex, "m_DiffuseIrradianceTex");
	NRI.SetDebugName(m_SpecularIrradianceTex, "m_SpecularIrradianceTex");

	std::vector<nri::TextureSubresourceUploadDesc> subResArray;
	subResArray.resize(diffuseIrradianceTex.GetMipNum() * diffuseIrradianceTex.GetArraySize());
	int subResCount = 0;
	for (size_t arrayIndex = 0; arrayIndex < diffuseIrradianceTex.GetArraySize(); arrayIndex++) {
		for (size_t mipIndex = 0; mipIndex < diffuseIrradianceTex.GetMipNum(); mipIndex++) {
			const tinyddsloader::DDSFile::ImageData *imgData = diffuseIrradianceTex.data.GetImageData(mipIndex, arrayIndex);
			subResArray[subResCount].slices = imgData->m_mem;
			subResArray[subResCount].sliceNum = 1;
			subResArray[subResCount].rowPitch = imgData->m_memPitch;
			subResArray[subResCount].slicePitch = imgData->m_memSlicePitch;
			subResCount++;
		}
	}

	nri::TextureUploadDesc textureData;
	textureData.subresources = subResArray.data();
	textureData.texture = m_DiffuseIrradianceTex;
	textureData.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
	textureData.planes = nri::PlaneBits::ALL;

	std::vector<nri::TextureSubresourceUploadDesc> subResArray1;
	subResArray1.resize(specularIrradianceTex.GetMipNum() * specularIrradianceTex.GetArraySize());
	subResCount = 0;
	for (size_t arrayIndex = 0; arrayIndex < specularIrradianceTex.GetArraySize(); arrayIndex++) {
		for (size_t mipIndex = 0; mipIndex < specularIrradianceTex.GetMipNum(); mipIndex++) {
			const tinyddsloader::DDSFile::ImageData *imgData = specularIrradianceTex.data.GetImageData(mipIndex, arrayIndex);
			subResArray1[subResCount].slices = imgData->m_mem;
			subResArray1[subResCount].sliceNum = 1;
			subResArray1[subResCount].rowPitch = imgData->m_memPitch;
			subResArray1[subResCount].slicePitch = imgData->m_memSlicePitch;
			subResCount++;
		}
	}

	nri::TextureUploadDesc textureData1;
	textureData1.subresources = subResArray1.data();
	textureData1.texture = m_SpecularIrradianceTex;
	textureData1.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
	textureData1.planes = nri::PlaneBits::ALL;

	std::vector<nri::TextureUploadDesc> texUploadDescArray = { textureData };
	NRI_ABORT_ON_FAILURE(NRI.UploadData(*m_GraphicsQueue, texUploadDescArray.data(), texUploadDescArray.size(),
			nullptr,
			0));

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
	simplePass->SetTestIndex(testIndex);
	simplePass->Render(info, camera);
}

void Renderer::OnPresent(RenderInfo &info) {
	Camera dummyCamera; // Create a dummy Camera object
	presentPass->Render(info, dummyCamera);
}
