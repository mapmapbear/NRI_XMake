#include "renderer.h"
#include "Camera.h"
#include "Utils.h"
#include "buffer.h"
#include "mesh.h"
#include "render_pass/commonMeshPass.h"
#include "render_pass/gridRenderPass.h"
#include "render_pass/instanceMeshPass.h"
#include "render_pass/presentPass.h"
#include "render_pass/skyRenderPass.h"
#include "texture.h"
#include <debugapi.h>
#include <memory>
#include <vector>

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
	descriptorPoolDesc.textureMaxNum = 1024;
	descriptorPoolDesc.samplerMaxNum = 10;

	NRI_ABORT_ON_FAILURE(NRI.CreateDescriptorPool(*m_Device, descriptorPoolDesc,
			m_DescriptorPool));
	NRI.SetDebugName(m_DescriptorPool, "m_DescriptorPool");

	// Init Defalut Resource
	utils::LoadTexture(utils::GetFullPath("black.png", utils::DataFolder::TEXTURES), defaultBlackTex);
	utils::LoadTexture(utils::GetFullPath("white.png", utils::DataFolder::TEXTURES), defaultWhiteTex);
	utils::LoadTexture(utils::GetFullPath("normal.png", utils::DataFolder::TEXTURES), defaultNormalTex);

	utils::LoadTexture(utils::GetFullPath("brdf.dds", utils::DataFolder::TEXTURES), BRDFTex, true);

	std::string sceneFile = utils::GetFullPath("Camera/Camera.gltf", utils::DataFolder::ROOT);
	//sceneFile = utils::GetFullPath("meshes/orrery/scene.gltf", utils::DataFolder::ROOT);
	sceneFile = utils::GetFullPath("Sponza/sponza.gltf", utils::DataFolder::ROOT);
	NRI_ABORT_ON_FALSE(utils::LoadScene(sceneFile, m_Scene, false));
	std::string diffuseIrrTex = "data/Textures/diffuseIrradiance.dds";
	std::string specularIrrTex = "data/Textures/specularIrradiance.dds";
#ifdef PBR_TEST
	diffuseIrrTex = "data/Textures/PBRTest/GrayDiffuse.dds";
	specularIrrTex = "data/Textures/PBRTest/GraySpecular.dds";
#endif

	if (!utils::LoadTexture(diffuseIrrTex, diffuseIrradianceTex, true)) {
		printf("Can not found this texture %s", diffuseIrrTex.c_str());
	}

	if (!utils::LoadTexture(specularIrrTex, specularIrradianceTex, true)) {
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

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = BRDFTex.GetFormat();
		textureDesc.width = BRDFTex.GetWidth();
		textureDesc.height = BRDFTex.GetHeight();
		textureDesc.mipNum = 1;

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_Device, textureDesc, m_BRDFTex));
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = defaultBlackTex.GetFormat();
		textureDesc.width = defaultBlackTex.GetWidth();
		textureDesc.height = defaultBlackTex.GetHeight();
		textureDesc.mipNum = 1;

		nri::Texture2DViewDesc texViewDesc = {};
		texViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D;
		texViewDesc.format = defaultBlackTex.GetFormat();

		m_DefaultBlackTex = std::make_shared<Texture>();
		m_DefaultBlackTex->Create(this, textureDesc, texViewDesc);
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = defaultWhiteTex.GetFormat();
		textureDesc.width = defaultWhiteTex.GetWidth();
		textureDesc.height = defaultWhiteTex.GetHeight();
		textureDesc.mipNum = 1;

		nri::Texture2DViewDesc texViewDesc = {};
		texViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D;
		texViewDesc.format = defaultWhiteTex.GetFormat();

		m_DefaultWhiteTex = std::make_shared<Texture>();
		m_DefaultWhiteTex->Create(this, textureDesc, texViewDesc);
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = defaultNormalTex.GetFormat();
		textureDesc.width = defaultNormalTex.GetWidth();
		textureDesc.height = defaultNormalTex.GetHeight();
		textureDesc.mipNum = 1;

		nri::Texture2DViewDesc texViewDesc = {};
		texViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D;
		texViewDesc.format = defaultNormalTex.GetFormat();

		m_DefaultNormalTex = std::make_shared<Texture>();
		m_DefaultNormalTex->Create(this, textureDesc, texViewDesc);
	}

	std::vector<nri::Texture *> textureArray = { m_DiffuseIrradianceTex, m_SpecularIrradianceTex, m_BRDFTex };
	nri::ResourceGroupDesc resourceGroupDesc = {};
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.textureNum = (uint32_t)textureArray.size();
	resourceGroupDesc.textures = textureArray.data();

	m_MemoryAllocations.resize(
			NRI.CalculateAllocationNumber(*m_Device, resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_Device, resourceGroupDesc, m_MemoryAllocations.data()));

	NRI.SetDebugName(m_DiffuseIrradianceTex, "m_DiffuseIrradianceTex");
	NRI.SetDebugName(m_SpecularIrradianceTex, "m_SpecularIrradianceTex");
	NRI.SetDebugName(m_BRDFTex, "m_BRDFTex");

	// Diffuse Irrandiance Texture

	std::vector<nri::TextureSubresourceUploadDesc> subResArray;
	subResArray.resize(diffuseIrradianceTex.GetMipNum() * diffuseIrradianceTex.GetArraySize());
	int subResCount = 0;
	for (size_t arrayIndex = 0; arrayIndex < diffuseIrradianceTex.GetArraySize(); arrayIndex++) {
		for (size_t mipIndex = 0; mipIndex < diffuseIrradianceTex.GetMipNum(); mipIndex++) {
			const tinyddsloader::DDSFile::ImageData *imgData = diffuseIrradianceTex.data.GetImageData((uint32_t)mipIndex, (uint32_t)arrayIndex);
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

	// Specular Irrandiance Texture

	std::vector<nri::TextureSubresourceUploadDesc> subResArray1;
	subResArray1.resize(specularIrradianceTex.GetMipNum() * specularIrradianceTex.GetArraySize());
	subResCount = 0;
	for (size_t arrayIndex = 0; arrayIndex < specularIrradianceTex.GetArraySize(); arrayIndex++) {
		for (size_t mipIndex = 0; mipIndex < specularIrradianceTex.GetMipNum(); mipIndex++) {
			const tinyddsloader::DDSFile::ImageData *imgData = specularIrradianceTex.data.GetImageData((uint32_t)mipIndex, (uint32_t)arrayIndex);
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

	// BRDF Texture
	nri::TextureSubresourceUploadDesc subResArray3;
	auto imgData = BRDFTex.data.GetImageData(0, 0);
	subResArray3.slices = imgData->m_mem;
	subResArray3.sliceNum = 1;
	subResArray3.rowPitch = imgData->m_memPitch;
	subResArray3.slicePitch = imgData->m_memSlicePitch;

	nri::TextureUploadDesc textureData2;
	textureData2.subresources = &subResArray3;
	textureData2.texture = m_BRDFTex;
	textureData2.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
	textureData2.planes = nri::PlaneBits::ALL;

	nri::TextureSubresourceUploadDesc subResArray4;
	defaultBlackTex.GetSubresource(subResArray4, 0);
	nri::TextureUploadDesc textureData3;
	textureData3.subresources = &subResArray4;
	textureData3.texture = m_DefaultBlackTex->GetTexture();
	textureData3.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
	textureData3.planes = nri::PlaneBits::ALL;

	nri::TextureSubresourceUploadDesc subResArray5;
	defaultWhiteTex.GetSubresource(subResArray5, 0);
	nri::TextureUploadDesc textureData4;
	textureData4.subresources = &subResArray5;
	textureData4.texture = m_DefaultWhiteTex->GetTexture();
	textureData4.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
	textureData4.planes = nri::PlaneBits::ALL;

	nri::TextureSubresourceUploadDesc subResArray6;
	defaultNormalTex.GetSubresource(subResArray6, 0);
	nri::TextureUploadDesc textureData5;
	textureData5.subresources = &subResArray6;
	textureData5.texture = m_DefaultNormalTex->GetTexture();
	textureData5.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
	textureData5.planes = nri::PlaneBits::ALL;

	std::vector<nri::TextureUploadDesc> texUploadDescArray = { textureData, textureData1, textureData2, textureData3, textureData4, textureData5 };
	NRI_ABORT_ON_FAILURE(GetNRI().UploadData(*m_GraphicsQueue, texUploadDescArray.data(), (uint32_t)texUploadDescArray.size(),
			nullptr,
			0));

	std::shared_ptr<Mesh> mesh = std::make_unique<Mesh>();
	std::string meshFile = utils::GetFullPath("USD_Sponza/sponza.usdc", utils::DataFolder::ROOT);
	meshFile = utils::GetFullPath("GLTF_Sponza/sponza.gltf", utils::DataFolder::ROOT);
	mesh->LoadFromUSD(meshFile, this);
	for (int i = 0; i < mesh->GetMeshCount(); ++i) {
		RenderNode node;
		node.mesh = mesh->GetMesh(i);
		node.material = &mesh->GetMaterial(node.mesh->GetMaterialID());
		node.globalTransform = node.mesh->GetTransform();

		if (node.material->IsTransparent) {
			m_TransparentRenderNodes.push_back(node);
		} else {
			m_OpaqueRenderNodes.push_back(node);
		}
	}
	UploadSceneData();
	skyPass = std::make_shared<SkyRenderPass>(this);
	gridPass = std::make_shared<GridRenderPass>(this);
	meshPass = std::make_shared<InstanceMeshPass>(this);
	simplePass = std::make_shared<CommonMeshPass>(this, m_Scene, mesh);
}

void Renderer::UploadSceneData() {
	std::vector<nri::TextureUploadDesc> uploadDesces(uploadTextureMap.size());
	std::vector<nri::TextureSubresourceUploadDesc> uploadSubDesces(uploadTextureMap.size());
	int i = 0;
	for (auto &node : uploadTextureMap) {
		std::shared_ptr<Texture> tex = node.first;
		std::shared_ptr<utils::Texture> texData = node.second;

		nri::TextureSubresourceUploadDesc &subRes = uploadSubDesces[i];
		texData->GetSubresource(subRes, 0);
		nri::TextureUploadDesc &textureData = uploadDesces[i++];
		textureData.subresources = &subRes;
		textureData.texture = tex->GetTexture();
		textureData.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
		textureData.planes = nri::PlaneBits::ALL;
	}

	i = 0;
	std::vector<nri::BufferUploadDesc> uploadBufferDescs;
	std::vector<std::vector<uint8_t>> geometryBufferDatas;
	uploadBufferDescs.resize(uploadIndexBufferMap.size() + uploadShadowIndexBufferMap.size());
	geometryBufferDatas.resize(uploadIndexBufferMap.size() + uploadShadowIndexBufferMap.size());
	for (auto &node : uploadIndexBufferMap) {
		std::shared_ptr<Buffer> buffer = node.first;
		std::shared_ptr<utils::MeshData> meshData = node.second;
		uint32_t indicesAlignSize = (uint32_t)helper::Align(helper::GetByteSizeOf(meshData->indices), 32);
		uint32_t vertexSize = (uint32_t)helper::GetByteSizeOf(meshData->m_vertexesData);
		geometryBufferDatas.at(i).resize(indicesAlignSize + vertexSize);

		memcpy(&geometryBufferDatas.at(i)[0], meshData->indices.data(), helper::GetByteSizeOf(meshData->indices));
		memcpy(&geometryBufferDatas.at(i)[indicesAlignSize], meshData->m_vertexesData.data(), vertexSize);

		nri::BufferUploadDesc &bufferData = uploadBufferDescs.at(i);
		bufferData.buffer = buffer->GetBuffer();
		bufferData.data = geometryBufferDatas.at(i).data();
		bufferData.dataSize = geometryBufferDatas.at(i).size();
		bufferData.after = { nri::AccessBits::INDEX_BUFFER |
			nri::AccessBits::VERTEX_BUFFER };
		GetNRI().SetDebugName(buffer->GetBuffer(), std::format("Buffer{}", i).c_str());
		i++;
	}
	i = uploadIndexBufferMap.size();
	for (auto &node : uploadShadowIndexBufferMap) {
		std::shared_ptr<Buffer> buffer = node.first;
		std::shared_ptr<utils::MeshData> meshData = node.second;
		uint32_t shadow_indicesAlignSize = (uint32_t)helper::Align(helper::GetByteSizeOf(meshData->shadow_indices), 32);
		uint32_t vertexSize = (uint32_t)helper::GetByteSizeOf(meshData->vertices);
		geometryBufferDatas.at(i).resize(shadow_indicesAlignSize + vertexSize);
		memcpy(&geometryBufferDatas.at(i)[0], meshData->shadow_indices.data(), helper::GetByteSizeOf(meshData->shadow_indices));
		memcpy(&geometryBufferDatas.at(i)[shadow_indicesAlignSize], meshData->m_vertexesData.data(), vertexSize);

		nri::BufferUploadDesc &bufferData = uploadBufferDescs.at(i);
		bufferData.buffer = buffer->GetBuffer();
		bufferData.data = geometryBufferDatas.at(i).data();
		bufferData.dataSize = geometryBufferDatas.at(i).size();
		bufferData.after = { nri::AccessBits::INDEX_BUFFER |
			nri::AccessBits::VERTEX_BUFFER };
		GetNRI().SetDebugName(buffer->GetBuffer(), std::format("Buffer{}", i).c_str());
		i++;
	}

	NRI_ABORT_ON_FAILURE(GetNRI().UploadData(*m_GraphicsQueue, uploadDesces.data(), static_cast<uint32_t>(uploadDesces.size()),
			uploadBufferDescs.data(),
			static_cast<uint32_t>(uploadBufferDescs.size())));
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
