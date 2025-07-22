#include "renderer.h"
#include "Camera.h"
#include "NRIDescs.h"
#include "Utils.h"
#include "buffer.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/matrix.hpp"
#include "mesh.h"
#include "render_pass/boxCullingPass.h"
#include "render_pass/commonMeshPass.h"
#include "render_pass/debugDrawPass.h"
#include "render_pass/gpucullingPass.h"
#include "render_pass/gridRenderPass.h"
#include "render_pass/instanceMeshPass.h"
#include "render_pass/presentPass.h"
#include "render_pass/skyRenderPass.h"
#include "render_pass/ssaoCompPass.h"
#include "spdlog/spdlog.h"
#include "texture.h"
#include <debugapi.h>
#include <algorithm>
#include <memory>
#include <random>
#include <vector>

Renderer::Renderer(NRIInterface &NRI, nri::Device *device, Camera &camera) :
		m_Device(device), m_NRI(NRI), m_Camera(camera) {
	NRI_ABORT_ON_FAILURE(NRI.GetQueue(*m_Device, nri::QueueType::GRAPHICS, 0, m_GraphicsQueue));
	NRI.SetDebugName(m_GraphicsQueue, "GraphicsQueue");

	NRI_ABORT_ON_FAILURE(NRI.GetQueue(*m_Device, nri::QueueType::COMPUTE, 0, m_ComputeQueue));
	NRI.SetDebugName(m_ComputeQueue, "ComputeQueue");

	nri::DescriptorPoolDesc descriptorPoolDesc = {};
	descriptorPoolDesc.descriptorSetMaxNum = BUFFERED_FRAME_MAX_NUM + 20;
	descriptorPoolDesc.constantBufferMaxNum = BUFFERED_FRAME_MAX_NUM + 3;
	descriptorPoolDesc.storageBufferMaxNum = 99;
	descriptorPoolDesc.storageTextureMaxNum = 99;
	descriptorPoolDesc.structuredBufferMaxNum = 99;
	descriptorPoolDesc.storageStructuredBufferMaxNum = 99;
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

	// std::string sceneFile = utils::GetFullPath("Camera/Camera.gltf", utils::DataFolder::ROOT);
	//sceneFile = utils::GetFullPath("meshes/orrery/scene.gltf", utils::DataFolder::ROOT);
	// sceneFile = utils::GetFullPath("Sponza/sponza.gltf", utils::DataFolder::ROOT);
	// NRI_ABORT_ON_FALSE(utils::LoadScene(sceneFile, m_Scene, false));
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

void Renderer::OnStart(nri::DescriptorSet *globalSet, nri::Texture *colorTex, nri::Texture *depthTex) {
	auto NRI = m_NRI;
	m_ColorTex = colorTex;
	m_DepthTex = depthTex;

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = diffuseIrradianceTex.GetFormat();
		textureDesc.width = diffuseIrradianceTex.GetWidth();
		textureDesc.height = diffuseIrradianceTex.GetHeight();
		textureDesc.mipNum = diffuseIrradianceTex.GetMipNum();
		textureDesc.layerNum = diffuseIrradianceTex.GetArraySize();

		// NRI_ABORT_ON_FAILURE(NRI.CreateTexture(*m_Device, textureDesc, m_DiffuseIrradianceTex));

		nri::Texture2DViewDesc texViewDesc = {};
		texViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_CUBE;
		texViewDesc.format = diffuseIrradianceTex.GetFormat();
		texViewDesc.mipNum = 1; //diffuseIrradianceTex.GetMipNum();
		texViewDesc.mipOffset = 0;
		texViewDesc.layerNum = 6;

		m_DefaultIrradianceTex = std::make_shared<Texture>();
		m_DefaultIrradianceTex->Create(this, textureDesc, texViewDesc);
		m_DefaultIrradianceTex->CreateView(this, texViewDesc);
		m_DefaultIrradianceTex->SetViewIndex(7);
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

		// NRI_ABORT_ON_FAILURE(NRI.CreateTexture(*m_Device, textureDesc, m_SpecularIrradianceTex));

		nri::Texture2DViewDesc texture2DViewDesc = { .viewType = nri::Texture2DViewType::SHADER_RESOURCE_CUBE, .format = specularIrradianceTex.GetFormat(), .mipOffset = 0, .mipNum = specularIrradianceTex.GetMipNum(), .layerOffset = 0, .layerNum = 6 };

		m_DefaultSpecularIrradianceTex = std::make_shared<Texture>();
		m_DefaultSpecularIrradianceTex->Create(this, textureDesc, texture2DViewDesc);
		m_DefaultSpecularIrradianceTex->CreateView(this, texture2DViewDesc);
		m_DefaultSpecularIrradianceTex->SetViewIndex(8);
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = BRDFTex.GetFormat();
		textureDesc.width = BRDFTex.GetWidth();
		textureDesc.height = BRDFTex.GetHeight();
		textureDesc.mipNum = 1;

		// NRI_ABORT_ON_FAILURE(NRI.CreateTexture(*m_Device, textureDesc, m_BRDFTex));

		nri::Texture2DViewDesc texture2DViewDesc = { .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = BRDFTex.GetFormat() };

		m_DefaultBRDFTex = std::make_shared<Texture>();
		m_DefaultBRDFTex->Create(this, textureDesc, texture2DViewDesc);
		m_DefaultBRDFTex->CreateView(this, texture2DViewDesc);
		m_DefaultBRDFTex->SetViewIndex(9);
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
		m_DefaultBlackTex->CreateView(this, texViewDesc);
		m_DefaultBlackTex->SetViewIndex(10);
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
		m_DefaultWhiteTex->CreateView(this, texViewDesc);
		m_DefaultWhiteTex->SetViewIndex(11);
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
		m_DefaultNormalTex->CreateView(this, texViewDesc);
		m_DefaultNormalTex->SetViewIndex(12);
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::DEPTH_STENCIL_ATTACHMENT | nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = nri::Format::D32_SFLOAT;
		textureDesc.width = 2048;
		textureDesc.height = 2048;
		textureDesc.mipNum = 1;
#ifdef RZ
		textureDesc.optimizedClearValue = { .depthStencil = { 0.0f, 0 } };
#else
		textureDesc.optimizedClearValue = { .depthStencil = { 1.0f, 0 } };
#endif

		nri::Texture2DViewDesc texViewDesc = {};
		texViewDesc.viewType = nri::Texture2DViewType::DEPTH_STENCIL_ATTACHMENT;
		texViewDesc.format = nri::Format::D32_SFLOAT;
		texViewDesc.mipNum = 1;

		m_ShadowMap = std::make_shared<Texture>();
		m_ShadowMap->Create(this, textureDesc, texViewDesc);
		m_ShadowMap->CreateView(this, texViewDesc);
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		// m_ShadowMap->CreateView(this, texViewDesc, 1);
		m_ShadowMap->SetViewIndex(13);
		NRI.SetDebugName(m_ShadowMap->GetTexture(), "m_ShadowMap");
	}

	// std::vector<nri::Texture *> textureArray = { m_DiffuseIrradianceTex, m_SpecularIrradianceTex, m_BRDFTex };
	// nri::ResourceGroupDesc resourceGroupDesc = {};
	// resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	// resourceGroupDesc.textureNum = (uint32_t)textureArray.size();
	// resourceGroupDesc.textures = textureArray.data();

	// m_MemoryAllocations.resize(
	// 		NRI.CalculateAllocationNumber(*m_Device, resourceGroupDesc), nullptr);
	// NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
	// 		*m_Device, resourceGroupDesc, m_MemoryAllocations.data()));

	NRI.SetDebugName(m_DefaultIrradianceTex->GetTexture(), "m_DiffuseIrradianceTex");
	NRI.SetDebugName(m_DefaultSpecularIrradianceTex->GetTexture(), "m_SpecularIrradianceTex");
	NRI.SetDebugName(m_DefaultBRDFTex->GetTexture(), "m_BRDFTex");

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
	textureData.texture = m_DefaultIrradianceTex->GetTexture();
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
	textureData1.texture = m_DefaultSpecularIrradianceTex->GetTexture();
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
	textureData2.texture = m_DefaultBRDFTex->GetTexture();
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
#ifdef RZ
	std::vector<float> data1(depthSubRes.slicePitch, 0.0);
#else
	std::vector<float> data1(depthSubRes.slicePitch, 1.0);
#endif
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

	// m_ShadowCamera.vForward = vec3(0.001, -1.0, 0.001);
	// m_ShadowCamera.vRight = vec3(1.0, 0.0, 0.0);
	// m_ShadowCamera.vUp = vec3(0.0, 1.0, 0.0);

	m_ShadowCamera.vForward = vec3(0.0, -1.0, 0.0);
	m_ShadowCamera.vRight = vec3(1.0, 0.0, 0.0);
	m_ShadowCamera.vUp = vec3(0.0, 0.0, 1.0);
	m_ShadowCamera.m_desc.aspectRatio = 1.0f;
	m_ShadowCamera.m_desc.horizontalFov = 45.0f;
	m_ShadowCamera.m_desc.nearZ = 0.1f;
	m_ShadowCamera.m_desc.farZ = 200.0f;

	m_ShadowCamera.state.mWorldToView = glm::lookAtLH(m_Camera.state.globalPosition + m_ShadowCamera.vForward, m_Camera.state.globalPosition, m_ShadowCamera.vUp);
#ifdef RZ
	m_ShadowCamera.state.mViewToClip = glm::perspectiveLH_ZO(m_ShadowCamera.m_desc.horizontalFov, m_ShadowCamera.m_desc.aspectRatio, m_ShadowCamera.m_desc.farZ, m_ShadowCamera.m_desc.nearZ);
#else
	m_ShadowCamera.state.mViewToClip = glm::perspectiveLH_ZO(m_ShadowCamera.m_desc.horizontalFov, m_ShadowCamera.m_ShadowCamera.aspectRatio, m_ShadowCamera.m_desc.nearZ, m_ShadowCamera.m_desc.farZ);
#endif

	std::shared_ptr<Mesh> mesh = std::make_unique<Mesh>();
	std::string meshFile = {};
	meshFile = utils::GetFullPath("GLTF_Sponza/sponza.gltf", utils::DataFolder::ROOT);
	// meshFile = utils::GetFullPath("GLTF_Bistro/bistro.gltf", utils::DataFolder::ROOT);
	// meshFile = utils::GetFullPath("cubes.gltf", utils::DataFolder::ROOT);
	// meshFile = utils::GetFullPath("GLTF_Bistro/bistro_Ground.gltf", utils::DataFolder::ROOT);
	// meshFile = utils::GetFullPath("GLTF_OW/ow.gltf", utils::DataFolder::ROOT);
	// meshFile = utils::GetFullPath("GLTF_Bistro/bistro_S1.gltf", utils::DataFolder::ROOT);
	// meshFile = utils::GetFullPath("GLTF_Bunny/bunny1.gltf", utils::DataFolder::ROOT);
	mesh->LoadFromUSD(meshFile, this, true);
	debugdrawPass = std::make_shared<DebugDrawPass>(this);
	uint32_t totalClusterCount = 0;
	glm::vec3 sceneMin = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 sceneMax = glm::vec3(std::numeric_limits<float>::lowest());
	for (int i = 0; i < mesh->m_GPUMesh->m_meshlet.size(); ++i) {
		for (int j = 0; j < mesh->m_GPUMesh->m_meshlet[i].m_drawArgs.size(); ++j) {
			RenderNode node;
			node.mesh = mesh->GetMesh(i);
			node.meshGPU = mesh->m_GPUMesh.get();
			node.drawArgs.indexNum = mesh->m_GPUMesh->m_meshlet[i].m_drawArgs[j].base.indexNum;
			node.drawArgs.instanceNum = 1;
			node.drawArgs.baseIndex = mesh->m_GPUMesh->m_meshlet[i].m_drawArgs[j].base.baseIndex;
			node.drawArgs.baseVertex = mesh->m_GPUMesh->m_meshlet[i].m_drawArgs[j].base.baseVertex;

			node.material = &mesh->GetMaterial(node.mesh->GetMaterialID());
			node.materialIndex = mesh->m_GPUMesh->m_meshlet[i].m_materialIndex < 0 ? UINT32_MAX : (uint32_t)mesh->m_GPUMesh->m_meshlet[i].m_materialIndex;
			try {
				node.globalTransform = mesh->results.at(i);
			} catch (const std::exception &e) {
				std::cout << "Error: " << e.what() << std::endl;
				node.globalTransform = glm::mat4(1.0f);
			}

			node.cluster_aabb = std::make_pair(*reinterpret_cast<glm::vec3 *>((mesh->m_GPUMesh->m_meshlet[i].m_bounds[j].center)), glm::vec3(mesh->m_GPUMesh->m_meshlet[i].m_bounds[j].radius));

			// if (node.material->IsTransparent) {
			// 	m_TransparentRenderNodes.push_back(node);
			// } else
			{
				m_OpaqueRenderNodes.push_back(node);
			}
			totalClusterCount++;
		}
	}

	for (int j = 0; j < mesh->m_GPUMesh->m_meshlet[49].m_drawArgs.size(); ++j) {
		glm::mat4 transMat = mesh->results.at(49);
		std::pair<glm::vec3, glm::vec3> cluster_aabb = std::make_pair(*reinterpret_cast<glm::vec3 *>((mesh->m_GPUMesh->m_meshlet[49].m_bounds[j].center)), glm::vec3(mesh->m_GPUMesh->m_meshlet[49].m_bounds[j].radius));
		glm::vec3 min = cluster_aabb.first - cluster_aabb.second;
		glm::vec3 max = cluster_aabb.first + cluster_aabb.second;

		min = transMat * glm::vec4(min, 1.0);
		max = transMat * glm::vec4(max, 1.0);

		sceneMin = glm::min(sceneMin, min);
		sceneMax = glm::max(sceneMax, max);

		glm::vec3 center = (min + max) * 0.5f;
		glm::vec3 extent = (max - min) * 0.5f;

		debugdrawPass->DrawBox(center, extent, glm::vec4(1.0));
	}

	for (int i = 0; i < m_OpaqueRenderNodes.size(); ++i) {
		RenderNode &node = m_OpaqueRenderNodes[i];
		glm::mat4 transMat = node.globalTransform;

		glm::vec3 min = glm::vec4(node.mesh->aabb.first, 1.0);
		glm::vec3 max = glm::vec4(node.mesh->aabb.second, 1.0);

		min = node.cluster_aabb.first - node.cluster_aabb.second;
		max = node.cluster_aabb.first + node.cluster_aabb.second;

		min = transMat * glm::vec4(min, 1.0);
		max = transMat * glm::vec4(max, 1.0);

		glm::vec3 center = (min + max) * 0.5f;
		glm::vec3 extent = (max - min) * 0.5f;

		debugdrawPass->DrawBox(center, extent, glm::vec4(1.0));
	}

	for (int i = 0; i < m_OpaqueRenderNodes.size(); ++i) {
		RenderNode &node = m_OpaqueRenderNodes[i];
		glm::mat4 transMat = node.globalTransform;
		glm::vec4 center = glm::vec4(node.mesh->aabb2.first, 1.0);
		center = transMat * center;
		glm::vec4 extent = glm::vec4(node.mesh->aabb2.second, 1.0);
		extent = transMat * extent;
		debugdrawPass->DrawSphere(center, std::max(extent.x, std::min(extent.y, extent.z)), glm::vec4(1.0));
	}
	debugdrawPass->DrawFrustum(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

	m_SceneAABB = std::make_pair(sceneMin, sceneMax);
	UploadSceneData();
	RandomLights();
	skyPass = std::make_shared<SkyRenderPass>(this);
	gridPass = std::make_shared<GridRenderPass>(this);
	meshPass = std::make_shared<InstanceMeshPass>(this);
	simplePass = std::make_shared<CommonMeshPass>(this, m_Scene, mesh);
	ssaoCompPass = std::make_shared<SSAOCompPass>(this);
	debugdrawPass->GenerateBoxBuffer();
	boxCullingPass = std::make_shared<BoxCullingPass>(this);
	gpuCullingPass = std::make_shared<GPUCullingPass>(this);
}

glm::mat4 Renderer::computeLightSpaceMatrix(const glm::vec3 &lightDir, float distance) {
	// Compute light position based on direction and distance
	glm::vec3 lightPos = -lightDir * distance;

	// Look at origin from light position
	glm::vec3 target(0.0f);

	// Choose up vector, avoiding degenerate cases
	glm::vec3 up(0.0f, 1.0f, 0.0f);
	if (glm::abs(glm::dot(lightDir, up)) > 0.99f) {
		up = glm::vec3(0.0f, 0.0f, 1.0f);
	}

	// Create view matrix looking from light position
	return glm::lookAt(lightPos, target, up);
}

glm::mat4 createLightViewMatrix(const glm::vec3 &eye, const glm::vec3 &direction, const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f)) {
	// 验证方向向量非零
	if (glm::length(direction) < 1e-6f) {
		throw std::invalid_argument("Direction vector cannot be zero");
	}

	// 计算目标点：eye + direction
	glm::vec3 target = eye + glm::normalize(direction);

	// 验证上向量与方向向量不共线
	glm::vec3 forward = glm::normalize(direction);
	if (glm::length(glm::cross(up, forward)) < 1e-6f) {
		throw std::invalid_argument("Up vector and direction are collinear");
	}

	// 使用 GLM 的 lookAt 函数构建视图矩阵
	return glm::lookAt(eye, target, up);
}

glm::quat getRotationQuaternion(const glm::vec3 &v1, const glm::vec3 &v2) {
	glm::vec3 n1 = glm::normalize(v1);
	glm::vec3 n2 = glm::normalize(v2);

	glm::vec3 axis = glm::cross(n1, n2);
	float dot = glm::dot(n1, n2);
	dot = glm::clamp(dot, -1.0f, 1.0f);
	float angle = glm::acos(dot);
	if (glm::length(axis) < 0.0001f) {
		if (dot > 0.9999f) {
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		} else if (dot < -0.9999f) {
			glm::vec3 ortho = glm::abs(n1.x) > glm::abs(n1.z) ? glm::vec3(-n1.y, n1.x, 0.0f) : glm::vec3(0.0f, -n1.z, n1.y);
			axis = glm::normalize(ortho);
			angle = glm::pi<float>();
		}
	} else {
		axis = glm::normalize(axis);
	}
	return glm::angleAxis(angle, axis);
}

void Renderer::OnUpdate(float deltaTime) {
	glm::vec3 shadowCamerPos = m_Camera.state.globalPosition;
	shadowCamerPos.y = 0;
	m_ShadowCamera.state.mWorldToView = glm::lookAtLH(shadowCamerPos + m_ShadowCamera.vForward * 2.0f, m_Camera.state.globalPosition, m_ShadowCamera.vUp);
	UpdateCascadeSplit();
}
#define SHADOW_MAP_CASCADE_COUNT 4
void Renderer::UpdateCascadeSplit() {
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = m_Camera.m_desc.nearZ;
	float farClip = m_Camera.m_desc.farZ;

	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}
	float lastSplitDist = 0.0f;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f, 1.0f, 0.0f),
			glm::vec3(1.0f, 1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, 1.0f, 1.0f),
			glm::vec3(1.0f, 1.0f, 1.0f),
			glm::vec3(1.0f, -1.0f, 1.0f),
			glm::vec3(-1.0f, -1.0f, 1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = (glm::inverse(m_ShadowCamera.state.mViewToClip * m_ShadowCamera.state.mWorldToView));
		for (uint32_t j = 0; j < 8; j++) {
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
			frustumCorners[j] = invCorner / invCorner.w;
		}

		for (uint32_t j = 0; j < 4; j++) {
			glm::vec3 dist = frustumCorners[j] - frustumCorners[j + 4];
			frustumCorners[j] = frustumCorners[j + 4] + (dist * splitDist);
			frustumCorners[j + 4] = frustumCorners[j + 4] + (dist * lastSplitDist);
		}

		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t j = 0; j < 8; j++) {
			frustumCenter += frustumCorners[j];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t j = 0; j < 8; j++) {
			float distance = glm::length(frustumCorners[j] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;
		vec3 lightPos = vec3(0.001, -1.0, 0.001);
		glm::vec3 lightDir = normalize(-lightPos);

		float shadowMapSize = 1024.0f;
		float texelSize = (maxExtents.x - minExtents.x) / shadowMapSize;

		glm::vec3 alignedCenter = frustumCenter;
		alignedCenter.x = floor(alignedCenter.x / texelSize) * texelSize;
		alignedCenter.y = floor(alignedCenter.y / texelSize) * texelSize;

		glm::mat4 lightViewMatrix = glm::lookAtLH(alignedCenter + lightDir * (maxExtents.z + 4.0f), frustumCenter, glm::vec3(0.0f, 0.0f, 1.0f));

		float alignedMinX = floor(minExtents.x / texelSize) * texelSize;
		float alignedMaxX = floor(maxExtents.x / texelSize) * texelSize;
		float alignedMinY = floor(minExtents.y / texelSize) * texelSize;
		float alignedMaxY = floor(maxExtents.y / texelSize) * texelSize;

		glm::mat4 lightOrthoMatrix = {};
#ifdef RZ
		lightOrthoMatrix = glm::orthoLH_ZO(alignedMinX, alignedMaxX, alignedMinY, alignedMaxY, maxExtents.z - minExtents.z, 0.0f);
#else
		lightOrthoMatrix = glm::orthoLH_ZO(alignedMinX, alignedMaxX, alignedMinY, alignedMaxY, 0.0f, maxExtents.z - minExtents.z);
#endif
		m_lightVP[i] = lightOrthoMatrix * lightViewMatrix;
		m_splitDepth[i] = (m_ShadowCamera.m_desc.nearZ + splitDist * clipRange);
		lastSplitDist = splitDist;
	}
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
		// bufferData.dataSize = geometryBufferDatas.at(i).size();
		bufferData.after = { nri::AccessBits::INDEX_BUFFER |
			nri::AccessBits::VERTEX_BUFFER };
		GetNRI().SetDebugName(buffer->GetBuffer(), std::format("Buffer{}", i).c_str());
		i++;
	}
	i = static_cast<int>(uploadIndexBufferMap.size());
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
		// bufferData.dataSize = geometryBufferDatas.at(i).size();
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
	// #ifdef SSAO_DEBUG
	if (m_config.DebugSSAOState) {
		presentPass = std::make_shared<PresentPass>(this, ssaoCompPass->m_SSAOTexture->GetTexture(), swawpchain);
	} else {
		presentPass = std::make_shared<PresentPass>(this, colorRT, swawpchain);
	}
}

void Renderer::OnRender(RenderInfo &info, Camera &camera) {
	nri::AttachmentsDesc depthAttachmentsDesc = info.desc;
	depthAttachmentsDesc.colorNum = 0;
	depthAttachmentsDesc.colors = nullptr;

	{
		nri::BufferBarrierDesc bufferBarrierDescs = {};
		bufferBarrierDescs.buffer = gpuCullingPass->m_CullGPUSceneObjectsBuffer->GetBuffer();
		bufferBarrierDescs.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
		bufferBarrierDescs.after = { nri::AccessBits::ARGUMENT_BUFFER, nri::StageBits::INDIRECT };
		nri::BarrierGroupDesc barrierGroupDesc = {};
		barrierGroupDesc.bufferNum = 1;
		barrierGroupDesc.buffers = &bufferBarrierDescs;
		GetNRI().CmdBarrier(info.cmdBuffer, barrierGroupDesc);
	}

	gpuCullingPass->Render(info, camera);

	GetNRI().CmdBeginRendering(info.cmdBuffer, depthAttachmentsDesc);
	{
		RenderInfo depthInfo = { .desc = depthAttachmentsDesc, .cmdBuffer = info.cmdBuffer };
		simplePass->RenderDepth(depthInfo, camera);
	}
	GetNRI().CmdEndRendering(info.cmdBuffer);

	gpuCullingPass->RenderHiZ(info);
	gpuCullingPass->RenderPost(info, camera);
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
		// meshPass->Render(info, camera);
		simplePass->SetTestIndex(testIndex);
		simplePass->Render(info, camera);
		if (m_config.DebugDrawState) {
			debugdrawPass->Render(info, camera);
		}
		// boxCullingPass->Render(info, camera);
	}
	GetNRI().CmdEndRendering(info.cmdBuffer);

	{
		nri::BufferBarrierDesc bufferBarrierDescs = {};
		bufferBarrierDescs.buffer = gpuCullingPass->m_CullGPUSceneObjectsBuffer->GetBuffer();
		bufferBarrierDescs.before = { nri::AccessBits::ARGUMENT_BUFFER, nri::StageBits::INDIRECT };
		nri::BarrierGroupDesc barrierGroupDesc = {};
		bufferBarrierDescs.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
		barrierGroupDesc.bufferNum = 1;
		barrierGroupDesc.buffers = &bufferBarrierDescs;
		GetNRI().CmdBarrier(info.cmdBuffer, barrierGroupDesc);
	}
}

void Renderer::OnRenderDepth(RenderInfo &info, Camera &camera) {
	// In this Pass, DepthTex is SRV state
	ssaoCompPass->Render(info, camera);
	{
		nri::TextureBarrierDesc textureBarrierDescs = {};
		textureBarrierDescs.texture = ssaoCompPass->m_SSAOTexture->GetTexture();
		textureBarrierDescs.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE,
			nri::Layout::SHADER_RESOURCE_STORAGE };
		textureBarrierDescs.after = { nri::AccessBits::SHADER_RESOURCE,
			nri::Layout::SHADER_RESOURCE };
		nri::BarrierGroupDesc barrierGroupDesc = {};
		barrierGroupDesc.textureNum = 1;
		barrierGroupDesc.textures = &textureBarrierDescs;
		GetNRI().CmdBarrier(info.cmdBuffer, barrierGroupDesc);
	}
}

void Renderer::OnPresent(RenderInfo &info) {
	Camera dummyCamera; // Create a dummy Camera object
	presentPass->Render(info, dummyCamera);

	{
		nri::TextureBarrierDesc textureBarrierDescs = {};
		textureBarrierDescs.texture = ssaoCompPass->m_SSAOTexture->GetTexture();
		textureBarrierDescs.before = { nri::AccessBits::SHADER_RESOURCE,
			nri::Layout::SHADER_RESOURCE };
		textureBarrierDescs.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE,
			nri::Layout::SHADER_RESOURCE_STORAGE };
		nri::BarrierGroupDesc barrierGroupDesc = {};
		barrierGroupDesc.textureNum = 1;
		barrierGroupDesc.textures = &textureBarrierDescs;
		GetNRI().CmdBarrier(info.cmdBuffer, barrierGroupDesc);
	}
}
