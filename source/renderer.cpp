#include "renderer.h"
#include "Camera.h"
#include "NRIDescs.h"
#include "Utils.h"
#include "buffer.h"
#include "mesh.h"
#include "render_pass/commonMeshPass.h"
#include "render_pass/gridRenderPass.h"
#include "render_pass/instanceMeshPass.h"
#include "render_pass/presentPass.h"
#include "render_pass/skyRenderPass.h"
#include "spdlog/spdlog.h"
#include "texture.h"
#include <debugapi.h>
#include <memory>
#include <random>
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

void Renderer::RandomLights() {
	// 使用场景包围盒的范围来限制光源位置
	glm::vec3 min = m_SceneAABB.first;
	glm::vec3 max = m_SceneAABB.second;

	// 设置随机数生成器
	std::random_device rd;
	std::mt19937 gen(rd());

	// 为每个维度创建均匀分布
	std::uniform_real_distribution<float> distX(min.x, max.x);
	std::uniform_real_distribution<float> distY(min.y, max.y);
	std::uniform_real_distribution<float> distZ(min.z, max.z);

	// 随机生成10个光源位置
	// for (int i = 0; i < 10; i++) {
	// 	glm::vec3 lightPos;
	// 	lightPos.x = distX(gen);
	// 	lightPos.y = distY(gen);
	// 	lightPos.z = distZ(gen);

	// 	m_LightPositions.push_back(lightPos);
	// }
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

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::DEPTH_STENCIL_ATTACHMENT | nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = nri::Format::D32_SFLOAT;
		textureDesc.width = 2048;
		textureDesc.height = 2048;
		textureDesc.mipNum = 1;

		nri::Texture2DViewDesc texViewDesc = {};
		texViewDesc.viewType = nri::Texture2DViewType::DEPTH_STENCIL_ATTACHMENT;
		texViewDesc.format = nri::Format::D32_SFLOAT;

		m_ShadowMap = std::make_shared<Texture>();
		m_ShadowMap->Create(this, textureDesc, texViewDesc);
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

	nri::TextureSubresourceUploadDesc depthSubRes = {};
	depthSubRes.rowPitch = 2048 * 4;
	depthSubRes.slicePitch = depthSubRes.rowPitch * 2048;
	std::vector<uint8_t> data1(depthSubRes.slicePitch, 1.0);
	depthSubRes.sliceNum = 1;
	depthSubRes.slices = data1.data();
	nri::TextureUploadDesc textureData6;
	textureData6.subresources = &depthSubRes;
	textureData6.texture = m_ShadowMap->GetTexture();
	textureData6.after = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT };
	textureData6.planes = nri::PlaneBits::ALL;

	std::vector<nri::TextureUploadDesc> texUploadDescArray = { textureData, textureData1, textureData2, textureData3, textureData4, textureData5, textureData6 };
	NRI_ABORT_ON_FAILURE(GetNRI().UploadData(*m_GraphicsQueue, texUploadDescArray.data(), (uint32_t)texUploadDescArray.size(),
			nullptr,
			0));

	std::shared_ptr<Mesh> mesh = std::make_unique<Mesh>();
	std::string meshFile = utils::GetFullPath("USD_Sponza/sponza.usdc", utils::DataFolder::ROOT);
	meshFile = utils::GetFullPath("GLTF_Sponza/sponza.gltf", utils::DataFolder::ROOT);
	mesh->LoadFromUSD(meshFile, this);

	glm::vec3 sceneMin = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 sceneMax = glm::vec3(std::numeric_limits<float>::lowest());
	for (int i = 0; i < mesh->GetMeshCount(); ++i) {
		RenderNode node;
		node.mesh = mesh->GetMesh(i);
		node.material = &mesh->GetMaterial(node.mesh->GetMaterialID());
		node.globalTransform = mesh->results.at(i);

		auto submeshAABB = node.mesh->aabb;
		glm::vec3 transformedMin = glm::vec3(node.globalTransform * glm::vec4(submeshAABB.first, 1.0f));
		glm::vec3 transformedMax = glm::vec3(node.globalTransform * glm::vec4(submeshAABB.second, 1.0f));

		sceneMin = glm::min(sceneMin, transformedMin);
		sceneMax = glm::max(sceneMax, transformedMax);

		if (node.material->IsTransparent) {
			m_TransparentRenderNodes.push_back(node);
		} else {
			m_OpaqueRenderNodes.push_back(node);
		}
	}
	m_SceneAABB = std::make_pair(sceneMin, sceneMax);

	RandomLights();
	UploadSceneData();
	skyPass = std::make_shared<SkyRenderPass>(this);
	gridPass = std::make_shared<GridRenderPass>(this);
	meshPass = std::make_shared<InstanceMeshPass>(this);
	simplePass = std::make_shared<CommonMeshPass>(this, m_Scene, mesh);
}

glm::mat4 Renderer::computeLightSpaceMatrix(float yaw, float pitch, float roll) {
	glm::vec3 defaultLightDir(0.0f, 1.0f, 0.0f);
	glm::mat4 rotationMatrix = glm::eulerAngleYXZ(glm::radians(yaw), glm::radians(pitch), glm::radians(roll));
	glm::vec4 rotatedLightDir = rotationMatrix * glm::vec4(defaultLightDir, 0.0f);
	glm::vec3 lightDir = glm::normalize(glm::vec3(rotatedLightDir));

	glm::vec3 eye(0.0f, 100.0f, 0.0f);
	glm::vec3 center = eye + lightDir;
	glm::vec3 up(0.0f, 1.0f, 0.0f);

	if (glm::length(glm::cross(lightDir, up)) < 1e-6f) {
		up = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	glm::mat4 lightSpaceMatrix = glm::lookAt(eye, center, up);

	return lightSpaceMatrix;
}

void Renderer::OnUpdate(float deltaTime) {
	m_lightPos = glm::vec3(cos(glm::radians(testVec.x)), 1.5f * 80.0f, cos(glm::radians(testVec.y)) * 1.0f);
	m_lightPos = glm::vec3(0.01f, 200.0f, 0.01f);
	glm::mat4 lightView = glm::lookAt(m_lightPos, glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0f, 1.0f, 0.0f));
	// lightView = computeLightSpaceMatrix(testVec.x, testVec.y, testVec.z);
	float orthoSize = 200.0f;
	glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 200.0f, 0.1f);
	m_lightVP = lightProj * lightView;
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
	nri::AttachmentsDesc depthAttachmentsDesc = info.desc;
	depthAttachmentsDesc.colorNum = 0;
	depthAttachmentsDesc.colors = nullptr;

	GetNRI().CmdBeginRendering(info.cmdBuffer, depthAttachmentsDesc);
	{
		RenderInfo depthInfo = { .desc = depthAttachmentsDesc, .cmdBuffer = info.cmdBuffer };
		simplePass->RenderDepth(depthInfo, camera);
	}
	GetNRI().CmdEndRendering(info.cmdBuffer);

	nri::AttachmentsDesc shadowAttachmentDesc = {};
	shadowAttachmentDesc.depthStencil = m_ShadowMap->GetView();
	GetNRI().CmdBeginRendering(info.cmdBuffer, shadowAttachmentDesc);
	{
		RenderInfo shadowInfo = { .desc = shadowAttachmentDesc, .cmdBuffer = info.cmdBuffer };
		simplePass->RenderShadow(shadowInfo, camera);
	}
	GetNRI().CmdEndRendering(info.cmdBuffer);

	GetNRI().CmdBeginRendering(info.cmdBuffer, info.desc);
	{
		skyPass->Render(info, camera);
		gridPass->Render(info, camera);
		meshPass->Render(info, camera);
		simplePass->SetTestIndex(testIndex);
		simplePass->Render(info, camera);
	}
	GetNRI().CmdEndRendering(info.cmdBuffer);
}

void Renderer::OnRenderDepth(RenderInfo &info, Camera &camera) {
	simplePass->Render(info, camera);
}

void Renderer::OnPresent(RenderInfo &info) {
	Camera dummyCamera; // Create a dummy Camera object
	presentPass->Render(info, dummyCamera);
}
