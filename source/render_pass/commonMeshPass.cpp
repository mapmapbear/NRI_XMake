#include "commonMeshPass.h"
#include "../renderer.h"
#include "NRIDescs.h"
#include "assimp/scene.h"
#include "buffer.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_projection.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/matrix.hpp"
#include "gpucullingPass.h"
#include "mesh.h"
#include "render_pass/presentPass.h"
#include "spdlog/spdlog.h"
#include "texture.h"
#include <stddef.h>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <vector>

struct CBlock {
	glm::mat4 modelMat;
	glm::vec4 camPos;
	glm::vec4 testVec;
	glm::vec4 baseColor;
	glm::vec4 pbrParams;
	uint32_t index[4];
};

CommonMeshPass::CommonMeshPass(Renderer *renderer, utils::Scene &scene, std::shared_ptr<Mesh> &rootMesh) :
		CommonRenderPass(renderer), m_Scene(scene), m_rootMesh(std::move(rootMesh)) {
	m_NRI = &m_renderer->GetNRI();
	// m_Scene = scene;
	auto NRI = *m_NRI;
	AllocGPUMemory();
	BindMemory();
	BuildPipeline();
}

void CommonMeshPass::AllocGPUMemory() {
	auto NRI = *m_NRI;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());

	uint32_t texSize = static_cast<uint32_t>(m_Scene.materialDatas.size());
	uint32_t matTexCount = 3;
	m_texureDatas.resize(texSize * matTexCount);
	m_textures.resize(texSize * matTexCount);

	// // GPU Resource
	const uint32_t constantBufferSize = helper::Align((uint32_t)sizeof(ConstantBufferLayout),
			deviceDesc.memoryAlignment.constantBufferOffset);

	{
		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = constantBufferSize * BUFFERED_FRAME_MAX_NUM;
		bufferDesc.usage = nri::BufferUsageBits::CONSTANT_BUFFER;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateBuffer(*m_renderer->GetRenderDevice(), bufferDesc, m_ConstantBuffer));
	}

	{
		m_indirectBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(nri::DrawIndexedDesc) * m_renderer->m_OpaqueRenderNodes.size();
		bufferDesc.usage = nri::BufferUsageBits::ARGUMENT_BUFFER | nri::BufferUsageBits::SHADER_RESOURCE;

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		viewDesc.format = nri::Format::R32_UINT;

		m_indirectBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_indirectBuffer->GetBuffer(), "indirectBuffer_GPUScene");
	}

	{
		m_worldMatBuffer = std::make_shared<Buffer>();
		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(glm::mat4) * m_renderer->m_OpaqueRenderNodes.size();
		bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
		bufferDesc.structureStride = sizeof(glm::mat4);

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		viewDesc.size = bufferDesc.size;

		m_worldMatBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_worldMatBuffer->GetBuffer(), "worldMatBuffer_GPUScene");
	}

	{
		m_sphereCullBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		uint32_t dataSize = sizeof(CullData);
		bufferDesc.size = dataSize * m_renderer->m_OpaqueRenderNodes.size();
		bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
		bufferDesc.structureStride = dataSize;

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		viewDesc.size = bufferDesc.size;

		m_sphereCullBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_sphereCullBuffer->GetBuffer(), "CullDataBuffer_GPUScene");
	}

	{
		std::vector<nri::DrawIndexedDesc> indirectBufferData;
		indirectBufferData.resize(m_renderer->m_OpaqueRenderNodes.size());

		std::vector<glm::mat4> worldMatData;
		std::vector<CullData> cullDatas;
		worldMatData.resize(m_renderer->m_OpaqueRenderNodes.size());
		cullDatas.resize(m_renderer->m_OpaqueRenderNodes.size());
		for (size_t i = 0; i < m_renderer->m_OpaqueRenderNodes.size(); ++i) {
			Renderer::RenderNode node = m_renderer->m_OpaqueRenderNodes[i];
			indirectBufferData[i].indexNum = node.drawArgs.indexNum;
			indirectBufferData[i].instanceNum = 1;
			indirectBufferData[i].baseIndex = node.drawArgs.baseIndex;
			indirectBufferData[i].baseVertex = node.drawArgs.baseVertex;
			indirectBufferData[i].baseInstance = (uint32_t)i;

			worldMatData[i] = node.globalTransform;
			cullDatas[i].center = node.mesh->aabb2.first;
			cullDatas[i].radians = std::max(node.mesh->aabb2.second.x, std::max(node.mesh->aabb2.second.y, node.mesh->aabb2.second.z));
		}

		nri::BufferUploadDesc bufferData = {};
		bufferData.buffer = m_indirectBuffer->GetBuffer();
		bufferData.data = indirectBufferData.data();
		bufferData.dataSize = sizeof(nri::DrawIndexedDesc) * indirectBufferData.size();
		bufferData.after = { .access = nri::AccessBits::ARGUMENT_BUFFER, .stages = nri::StageBits::INDIRECT };

		nri::BufferUploadDesc bufferData2 = {};
		bufferData2.buffer = m_worldMatBuffer->GetBuffer();
		bufferData2.data = worldMatData.data();
		bufferData2.dataSize = helper::GetByteSizeOf(worldMatData);
		bufferData2.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::VERTEX_SHADER };

		nri::BufferUploadDesc bufferData3 = {};
		bufferData3.buffer = m_sphereCullBuffer->GetBuffer();
		bufferData3.data = cullDatas.data();
		bufferData3.dataSize = sizeof(CullData) * cullDatas.size();
		bufferData3.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::VERTEX_SHADER };

		std::vector<nri::BufferUploadDesc> uploadDescArray = { bufferData, bufferData2, bufferData3 };

		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), nullptr, 0,
				uploadDescArray.data(),
				(uint32_t)uploadDescArray.size()));
	}
}

void CommonMeshPass::BindMemory() {
	auto NRI = *m_NRI;
	// Bind Memory
	std::vector<nri::Buffer *> constantBufferArray = { m_ConstantBuffer };

	nri::ResourceGroupDesc resourceGroupDesc = {};
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::HOST_UPLOAD;
	resourceGroupDesc.bufferNum = (uint32_t)constantBufferArray.size();
	resourceGroupDesc.buffers = constantBufferArray.data();

	m_MemoryAllocations.resize(1, nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(*m_renderer->GetRenderDevice(), resourceGroupDesc,
			m_MemoryAllocations.data()));
	// Descriptors
	for (size_t i = 0; i < m_rootMesh->GetMeshCount(); ++i) {
		const SubMesh *subMesh = m_rootMesh->GetMesh(static_cast<uint32_t>(i));
		const Material &mat = m_rootMesh->GetMaterial(subMesh->GetMaterialID());
		m_matTexSet.insert(mat.m_BaseTexture);
		m_matTexSet.insert(mat.m_NormalTexture);
		m_matTexSet.insert(mat.m_MetallicTexture);
	}

	m_textureViews.resize(4);

	m_brdfTexIndex = m_renderer->texViewOffset;
	nri::Texture2DViewDesc texture2DViewDesc1 = { .texture = m_renderer->m_DiffuseIrradianceTex, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_CUBE, .format = m_renderer->diffuseIrradianceTex.GetFormat(), .mipOffset = 0, .mipNum = m_renderer->diffuseIrradianceTex.GetMipNum(), .layerOffset = 0, .layerNum = 6 };

	NRI_ABORT_ON_FAILURE(
			NRI.CreateTexture2DView(texture2DViewDesc1, m_textureViews[0]));

	nri::Texture2DViewDesc texture2DViewDesc2 = { .texture = m_renderer->m_SpecularIrradianceTex, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_CUBE, .format = m_renderer->specularIrradianceTex.GetFormat(), .mipOffset = 0, .mipNum = m_renderer->specularIrradianceTex.GetMipNum(), .layerOffset = 0, .layerNum = 6 };

	NRI_ABORT_ON_FAILURE(
			NRI.CreateTexture2DView(texture2DViewDesc2, m_textureViews[1]));

	nri::Texture2DViewDesc texture2DViewDes3 = { .texture = m_renderer->m_BRDFTex, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = m_renderer->BRDFTex.GetFormat() };
	NRI_ABORT_ON_FAILURE(
			NRI.CreateTexture2DView(texture2DViewDes3, m_textureViews[2]));

	nri::Texture2DViewDesc texture2DViewDes4 = { .texture = m_renderer->m_ShadowMap->GetTexture(), .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = nri::Format::D32_SFLOAT };
	NRI_ABORT_ON_FAILURE(
			NRI.CreateTexture2DView(texture2DViewDes4, m_textureViews[3]));

	{ // Sampler
		nri::SamplerDesc samplerDesc = {};
		samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE,
			nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
		samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR,
			nri::Filter::LINEAR };
		samplerDesc.mipMax = 16;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateSampler(*m_renderer->GetRenderDevice(), samplerDesc, m_Sampler));
	}
	NRI.SetDebugName(m_Sampler, "m_cubeSampler");

	// Shadow Sampler
	{
		nri::SamplerDesc samplerDesc = {};
		samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE,
			nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
		samplerDesc.filters = { nri::Filter::NEAREST, nri::Filter::NEAREST,
			nri::Filter::NEAREST };
		samplerDesc.compareFunc = nri::CompareFunc::LESS_EQUAL;
		samplerDesc.mipMin = 0;
		samplerDesc.mipMax = 1;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateSampler(*m_renderer->GetRenderDevice(), samplerDesc, m_SamplerShadow));
	}
	NRI.SetDebugName(m_SamplerShadow, "m_ShadowMapSampler");

	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());
	const uint32_t constantBufferSize = helper::Align((uint32_t)sizeof(ConstantBufferLayout),
			deviceDesc.memoryAlignment.constantBufferOffset);

	{
		nri::BufferViewDesc bufferViewDesc = {};
		bufferViewDesc.buffer = m_ConstantBuffer;
		bufferViewDesc.viewType = nri::BufferViewType::CONSTANT;
		bufferViewDesc.offset = 0;
		bufferViewDesc.size = constantBufferSize;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateBufferView(bufferViewDesc, m_ConstantBufferView));
		SPDLOG_INFO("texOffset= {}\n", m_renderer->texViewOffset++);
	}
}

void CommonMeshPass::BuildPipeline() {
	auto NRI = *m_NRI;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());

	// Pipeline
	{
		nri::DescriptorRangeDesc descriptorRangeConstant[1];
		descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER,
			nri::StageBits::ALL };

		nri::DescriptorRangeDesc descriptorRangeTexture[3];
		// descriptorRangeTexture[0] = { 0, (uint32_t)m_textureViews.size(), nri::DescriptorType::TEXTURE,
		descriptorRangeTexture[0] = { 0, 999, nri::DescriptorType::TEXTURE,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[1] = { 0, 2, nri::DescriptorType::SAMPLER,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[2] = { 0, 1, nri::DescriptorType::STRUCTURED_BUFFER,
			nri::StageBits::VERTEX_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeConstant,
					helper::GetCountOf(descriptorRangeConstant) },
			{ 1, descriptorRangeTexture, helper::GetCountOf(descriptorRangeTexture) },
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(CBlock),
			nri::StageBits::ALL };

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.descriptorSetNum =
				helper::GetCountOf(descriptorSetDescs);
		pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
		pipelineLayoutDesc.rootConstantNum = 1;
		pipelineLayoutDesc.rootConstants = &rootConstant;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_PipelineLayout));

		nri::VertexStreamDesc vertexStreamDesc = {};
		vertexStreamDesc.bindingSlot = 0;

		nri::VertexAttributeDesc vertexAttributeDesc[4] = {};
		{
			vertexAttributeDesc[0].format = nri::Format::RGB32_SFLOAT;
			vertexAttributeDesc[0].streamIndex = 0;
			vertexAttributeDesc[0].offset = helper::GetOffsetOf(&utils::Vertex::position);
			vertexAttributeDesc[0].d3d = { "POSITION", 0 };
			vertexAttributeDesc[0].vk.location = { 0 };

			vertexAttributeDesc[1].format = nri::Format::RG32_SFLOAT;
			vertexAttributeDesc[1].streamIndex = 0;
			vertexAttributeDesc[1].offset = helper::GetOffsetOf(&utils::Vertex::uv);
			vertexAttributeDesc[1].d3d = { "TEXCOORD", 0 };
			vertexAttributeDesc[1].vk.location = { 1 };

			vertexAttributeDesc[2].format = nri::Format::RGB32_SFLOAT;
			vertexAttributeDesc[2].streamIndex = 0;
			vertexAttributeDesc[2].offset = helper::GetOffsetOf(&utils::Vertex::normal);
			vertexAttributeDesc[2].d3d = { "NORMAL", 0 };
			vertexAttributeDesc[2].vk.location = { 2 };

			vertexAttributeDesc[3].format = nri::Format::RGB32_SFLOAT;
			vertexAttributeDesc[3].streamIndex = 0;
			vertexAttributeDesc[3].offset = helper::GetOffsetOf(&utils::Vertex::tangent);
			vertexAttributeDesc[3].d3d = { "TANGENT", 0 };
			vertexAttributeDesc[3].vk.location = { 3 };
		}

		nri::VertexInputDesc vertexInputDesc = {};
		vertexInputDesc.attributes = vertexAttributeDesc;
		vertexInputDesc.attributeNum =
				(uint8_t)helper::GetCountOf(vertexAttributeDesc);
		vertexInputDesc.streams = &vertexStreamDesc;
		vertexInputDesc.streamNum = 1;

		nri::InputAssemblyDesc inputAssemblyDesc = {};
		inputAssemblyDesc.topology = nri::Topology::TRIANGLE_LIST;

		nri::RasterizationDesc rasterizationDesc = {};
		rasterizationDesc.fillMode = nri::FillMode::SOLID;
		rasterizationDesc.cullMode = nri::CullMode::NONE;
		// rasterizationDesc.frontCounterClockwise = true;
		rasterizationDesc.depthClamp = true;

		nri::ColorAttachmentDesc colorAttachmentDesc = {};
#ifdef HDR_ENABLE
		colorAttachmentDesc.format = nri::Format::R10_G10_B10_A2_UNORM;
#else
		colorAttachmentDesc.format = nri::Format::RGBA8_UNORM;
#endif
		colorAttachmentDesc.colorWriteMask = nri::ColorWriteBits::RGBA;
		colorAttachmentDesc.blendEnabled = false;
		colorAttachmentDesc.colorBlend = { nri::BlendFactor::SRC_ALPHA,
			nri::BlendFactor::ONE_MINUS_SRC_ALPHA,
			nri::BlendFunc::ADD };

		nri::DepthAttachmentDesc depthAttachmentDesc = {};
		depthAttachmentDesc.write = false;
#ifdef RZ
		depthAttachmentDesc.compareFunc = nri::CompareFunc::GREATER_EQUAL;
#else
		depthAttachmentDesc.compareFunc = nri::CompareFunc::LESS_EQUAL;
#endif
		depthAttachmentDesc.boundsTest = false;

		nri::OutputMergerDesc outputMergerDesc = {};
		outputMergerDesc.colors = &colorAttachmentDesc;
		outputMergerDesc.colorNum = 1;
		outputMergerDesc.depth = depthAttachmentDesc;
		outputMergerDesc.depthStencilFormat = nri::Format::D32_SFLOAT;

		utils::ShaderCodeStorage shaderCodeStorage;

		nri::ShaderDesc shaderStages[] = {
			utils::LoadShader(deviceDesc.graphicsAPI,
					"simpleModelMesh.vs", shaderCodeStorage),
			utils::LoadShader(deviceDesc.graphicsAPI, "simpleModelMesh.fs",
					shaderCodeStorage),
		};

		nri::GraphicsPipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.pipelineLayout = m_PipelineLayout;
		graphicsPipelineDesc.vertexInput = &vertexInputDesc;
		graphicsPipelineDesc.inputAssembly = inputAssemblyDesc;
		graphicsPipelineDesc.rasterization = rasterizationDesc;
		graphicsPipelineDesc.outputMerger = outputMergerDesc;
		graphicsPipelineDesc.shaders = shaderStages;
		graphicsPipelineDesc.shaderNum = helper::GetCountOf(shaderStages);

		NRI_ABORT_ON_FAILURE(NRI.CreateGraphicsPipeline(
				*m_renderer->GetRenderDevice(), graphicsPipelineDesc, m_Pipeline));
	}

	// Depthonly Pipeline
	{
		nri::DescriptorRangeDesc descriptorRangeConstant[1];
		descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER,
			nri::StageBits::ALL };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeConstant,
					helper::GetCountOf(descriptorRangeConstant) }
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(CBlock),
			nri::StageBits::ALL };

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.descriptorSetNum =
				helper::GetCountOf(descriptorSetDescs);
		pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
		pipelineLayoutDesc.rootConstantNum = 1;
		pipelineLayoutDesc.rootConstants = &rootConstant;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_DepthPipelineLayout));

		nri::VertexStreamDesc vertexStreamDesc = {};
		vertexStreamDesc.bindingSlot = 0;

		nri::VertexAttributeDesc vertexAttributeDesc = {};
		vertexAttributeDesc.format = nri::Format::RGB32_SFLOAT;
		vertexAttributeDesc.streamIndex = 0;
		vertexAttributeDesc.offset = helper::GetOffsetOf(&utils::Vertex::position);
		vertexAttributeDesc.d3d = { "POSITION", 0 };
		vertexAttributeDesc.vk.location = { 0 };

		nri::VertexInputDesc vertexInputDesc = {};
		vertexInputDesc.attributes = &vertexAttributeDesc;
		vertexInputDesc.attributeNum = 1;
		vertexInputDesc.streams = &vertexStreamDesc;
		vertexInputDesc.streamNum = 1;

		nri::InputAssemblyDesc inputAssemblyDesc = {};
		inputAssemblyDesc.topology = nri::Topology::TRIANGLE_LIST;

		nri::RasterizationDesc rasterizationDesc = {};
		rasterizationDesc.fillMode = nri::FillMode::SOLID;
		rasterizationDesc.cullMode = nri::CullMode::NONE;
		// rasterizationDesc.frontCounterClockwise = false;
		// rasterizationDesc.depthClamp = true;

		nri::ColorAttachmentDesc colorAttachmentDesc = {};
#ifdef HDR_ENABLE
		colorAttachmentDesc.format = nri::Format::R10_G10_B10_A2_UNORM;
#else
		colorAttachmentDesc.format = nri::Format::RGBA8_UNORM;
#endif
		colorAttachmentDesc.colorWriteMask = nri::ColorWriteBits::RGBA;
		colorAttachmentDesc.blendEnabled = false;
		colorAttachmentDesc.colorBlend = { nri::BlendFactor::SRC_ALPHA,
			nri::BlendFactor::ONE_MINUS_SRC_ALPHA,
			nri::BlendFunc::ADD };

		nri::DepthAttachmentDesc depthAttachmentDesc = {};
		depthAttachmentDesc.write = true;
#ifdef RZ
		depthAttachmentDesc.compareFunc = nri::CompareFunc::GREATER_EQUAL;
#else
		depthAttachmentDesc.compareFunc = nri::CompareFunc::LESS_EQUAL;
#endif
		depthAttachmentDesc.boundsTest = false;

		nri::OutputMergerDesc outputMergerDesc = {};
		outputMergerDesc.depth = depthAttachmentDesc;
		outputMergerDesc.depthStencilFormat = nri::Format::D32_SFLOAT;

		utils::ShaderCodeStorage shaderCodeStorage;

		nri::ShaderDesc shaderStages[] = {
			utils::LoadShader(deviceDesc.graphicsAPI,
					"simpleModelMesh.vs_7AE4B116", shaderCodeStorage),
			utils::LoadShader(deviceDesc.graphicsAPI, "depthOnly",
					shaderCodeStorage, "ps_main")
		};

		nri::GraphicsPipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.pipelineLayout = m_DepthPipelineLayout;
		graphicsPipelineDesc.vertexInput = &vertexInputDesc;
		graphicsPipelineDesc.inputAssembly = inputAssemblyDesc;
		graphicsPipelineDesc.rasterization = rasterizationDesc;
		graphicsPipelineDesc.outputMerger = outputMergerDesc;
		graphicsPipelineDesc.shaders = shaderStages;
		graphicsPipelineDesc.shaderNum = helper::GetCountOf(shaderStages);

		NRI_ABORT_ON_FAILURE(NRI.CreateGraphicsPipeline(
				*m_renderer->GetRenderDevice(), graphicsPipelineDesc, m_DepthPipeline));
	}

	// Shadow Pipeline
	{
		nri::DescriptorRangeDesc descriptorRangeConstant[1];
		descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER,
			nri::StageBits::ALL };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeConstant,
					helper::GetCountOf(descriptorRangeConstant) }
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(CBlock),
			nri::StageBits::ALL };

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.descriptorSetNum =
				helper::GetCountOf(descriptorSetDescs);
		pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
		pipelineLayoutDesc.rootConstantNum = 1;
		pipelineLayoutDesc.rootConstants = &rootConstant;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_ShadowPipelineLayout));

		nri::VertexStreamDesc vertexStreamDesc = {};
		vertexStreamDesc.bindingSlot = 0;

		nri::VertexAttributeDesc vertexAttributeDesc = {};
		vertexAttributeDesc.format = nri::Format::RGB32_SFLOAT;
		vertexAttributeDesc.streamIndex = 0;
		vertexAttributeDesc.offset = helper::GetOffsetOf(&utils::Vertex::position);
		vertexAttributeDesc.d3d = { "POSITION", 0 };
		vertexAttributeDesc.vk.location = { 0 };

		nri::VertexInputDesc vertexInputDesc = {};
		vertexInputDesc.attributes = &vertexAttributeDesc;
		vertexInputDesc.attributeNum = 1;
		vertexInputDesc.streams = &vertexStreamDesc;
		vertexInputDesc.streamNum = 1;

		nri::InputAssemblyDesc inputAssemblyDesc = {};
		inputAssemblyDesc.topology = nri::Topology::TRIANGLE_LIST;

		nri::RasterizationDesc rasterizationDesc = {};
		rasterizationDesc.fillMode = nri::FillMode::SOLID;
		rasterizationDesc.cullMode = nri::CullMode::BACK;
		// rasterizationDesc.frontCounterClockwise = false;
		rasterizationDesc.depthClamp = true;

		nri::ColorAttachmentDesc colorAttachmentDesc = {};
#ifdef HDR_ENABLE
		colorAttachmentDesc.format = nri::Format::R10_G10_B10_A2_UNORM;
#else
		colorAttachmentDesc.format = nri::Format::RGBA8_UNORM;
#endif
		colorAttachmentDesc.colorWriteMask = nri::ColorWriteBits::RGBA;
		colorAttachmentDesc.blendEnabled = false;
		colorAttachmentDesc.colorBlend = { nri::BlendFactor::SRC_ALPHA,
			nri::BlendFactor::ONE_MINUS_SRC_ALPHA,
			nri::BlendFunc::ADD };

		nri::DepthAttachmentDesc depthAttachmentDesc = {};
		depthAttachmentDesc.write = true;
		depthAttachmentDesc.compareFunc = nri::CompareFunc::LESS_EQUAL;
		depthAttachmentDesc.boundsTest = false;

		nri::OutputMergerDesc outputMergerDesc = {};
		outputMergerDesc.depth = depthAttachmentDesc;
		outputMergerDesc.depthStencilFormat = nri::Format::D32_SFLOAT;

		utils::ShaderCodeStorage shaderCodeStorage;

		nri::ShaderDesc shaderStages[] = {
			utils::LoadShader(deviceDesc.graphicsAPI,
					"depthOnly", shaderCodeStorage, "vs_main"),
			utils::LoadShader(deviceDesc.graphicsAPI, "depthOnly",
					shaderCodeStorage, "ps_main")
		};

		nri::GraphicsPipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.pipelineLayout = m_ShadowPipelineLayout;
		graphicsPipelineDesc.vertexInput = &vertexInputDesc;
		graphicsPipelineDesc.inputAssembly = inputAssemblyDesc;
		graphicsPipelineDesc.rasterization = rasterizationDesc;
		graphicsPipelineDesc.outputMerger = outputMergerDesc;
		graphicsPipelineDesc.shaders = shaderStages;
		graphicsPipelineDesc.shaderNum = helper::GetCountOf(shaderStages);

		NRI_ABORT_ON_FAILURE(NRI.CreateGraphicsPipeline(
				*m_renderer->GetRenderDevice(), graphicsPipelineDesc, m_ShadowPipeline));
	}

	// add temp descriptors
	uint32_t newMatTexIndex = m_brdfTexIndex + 4;
	{
		for (auto &tex : m_matTexSet) {
			nri::Descriptor *view = tex->GetView();
			tex->SetViewIndex(newMatTexIndex++);
			m_textureViews.push_back(view);
		}
	}

	// Descriptor sets
	{
		// Texture
		NRI_ABORT_ON_FAILURE(
				NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_PipelineLayout, 1,
						&m_TextureDescriptorSet, 1, 0));

		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[3] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = (uint32_t)m_textureViews.size();
		descriptorRangeUpdateDescs[0].descriptors = m_textureViews.data();
		std::vector<nri::Descriptor *> samplerArray = { m_Sampler, m_SamplerShadow };
		descriptorRangeUpdateDescs[1].descriptorNum = 2;
		descriptorRangeUpdateDescs[1].descriptors = samplerArray.data();
		descriptorRangeUpdateDescs[2].descriptorNum = 1;
		nri::Descriptor *worldMatView = m_worldMatBuffer->GetView();
		descriptorRangeUpdateDescs[2].descriptors = &worldMatView;

		NRI.UpdateDescriptorRanges(*m_TextureDescriptorSet, 0,
				helper::GetCountOf(descriptorRangeUpdateDescs),
				descriptorRangeUpdateDescs);

		NRI_ABORT_ON_FAILURE(
				NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_PipelineLayout, 0,
						&m_ConstantBufferDescriptorSet, 1, 0));

		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDesc = {
			&m_ConstantBufferView, 1
		};
		NRI.UpdateDescriptorRanges(*m_ConstantBufferDescriptorSet, 0, 1,
				&descriptorRangeUpdateDesc);
	}
}

void CommonMeshPass::Render(RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;
	const glm::vec3 cameraPos = camera.state.globalPosition;
	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Forward Mesh Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		NRI.CmdSetDescriptorSet(info.cmdBuffer, 0,
				*m_ConstantBufferDescriptorSet, nullptr);
		{
			const nri::Viewport viewport = { 0.0f, 0.0f, (float)m_renderer->m_OutputResolution.first,
				(float)m_renderer->m_OutputResolution.second, 0.0f, 1.0f };
			NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

			nri::Rect scissor = { 0, 0, (uint16_t)m_renderer->m_OutputResolution.first, (uint16_t)m_renderer->m_OutputResolution.second };
			NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
		}

		nri::Buffer *geoBuffer = m_renderer->m_OpaqueRenderNodes[0].meshGPU->m_vertexbuffer->GetBuffer();
		nri::VertexBufferDesc vertexBufferDesc = {};
		vertexBufferDesc.buffer = geoBuffer;
		vertexBufferDesc.stride = sizeof(utils::Vertex);
		NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
		nri::Buffer *indexGeoBuffer = m_renderer->m_OpaqueRenderNodes[0].meshGPU->m_indexbuffer->GetBuffer();
		NRI.CmdSetIndexBuffer(info.cmdBuffer, *indexGeoBuffer, 0,
				nri::IndexType::UINT32);
		if (!m_renderer->m_config.IndirectDrawState) {
			for (uint32_t index = 0; index < m_renderer->m_OpaqueRenderNodes.size(); ++index) {
				Renderer::RenderNode &node = m_renderer->m_OpaqueRenderNodes[index];
				CBlock block = {};
				block.modelMat = node.globalTransform;
				block.camPos = vec4(cameraPos, 1.0);
				block.index[0] = node.material->m_BaseTexture->GetViewIndex();
				block.index[1] = block.index[2] = 0u;
				block.index[3] = m_brdfTexIndex;
				NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(CBlock));
				uint32_t instanceCount = 1;

				NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(node.drawArgs.indexNum), instanceCount, node.drawArgs.baseIndex, node.drawArgs.baseVertex, index });
			}
		} else {
			nri::Buffer *indirectBuffer = m_renderer->gpuCullingPass->m_CullGPUSceneObjectsBuffer->GetBuffer();
			NRI.CmdDrawIndexedIndirect(info.cmdBuffer, *indirectBuffer, 0, (uint32_t)m_renderer->m_OpaqueRenderNodes.size(), sizeof(nri::DrawIndexedDesc), nullptr, 0);
		}
	}
}

void CommonMeshPass::RenderDepth(RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;
	const glm::mat4 p = camera.statePrev.mViewToClip;
	const glm::vec3 cameraPos = camera.statePrev.position;

	ConstantBufferLayout *commonConstants = (ConstantBufferLayout *)NRI.MapBuffer(
			*m_ConstantBuffer, 0,
			sizeof(ConstantBufferLayout));
	if (commonConstants) {
		commonConstants->modelMat = glm::mat4(1.0);
		commonConstants->viewMat = camera.state.mWorldToView;
		commonConstants->projectMat = camera.state.mViewToClip;
		commonConstants->lightVP = m_renderer->m_lightVP;
		NRI.UnmapBuffer(*m_ConstantBuffer);
	}

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Depth PreZ Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_DepthPipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_DepthPipeline);
		NRI.CmdSetDescriptorSet(info.cmdBuffer, 0,
				*m_ConstantBufferDescriptorSet, nullptr);
		{
			const nri::Viewport viewport = { 0.0f, 0.0f, (float)m_renderer->m_OutputResolution.first,
				(float)m_renderer->m_OutputResolution.second, 0.0f, 1.0f };
			NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

			nri::Rect scissor = { 0, 0, (uint16_t)m_renderer->m_OutputResolution.first, (uint16_t)m_renderer->m_OutputResolution.second };
			NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
		}

		nri::Buffer *geoBuffer = m_renderer->m_OpaqueRenderNodes[0].meshGPU->m_vertexbuffer->GetBuffer();
		nri::VertexBufferDesc vertexBufferDesc = {};
		vertexBufferDesc.buffer = geoBuffer;
		vertexBufferDesc.stride = sizeof(utils::Vertex);
		NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
		nri::Buffer *indexGeoBuffer = m_renderer->m_OpaqueRenderNodes[0].meshGPU->m_indexbuffer->GetBuffer();
		NRI.CmdSetIndexBuffer(info.cmdBuffer, *indexGeoBuffer, 0,
				nri::IndexType::UINT32);
		// if (m_renderer->m_config.IndirectDrawState) 
		{
			for (uint32_t index = 0; index < m_renderer->m_OpaqueRenderNodes.size(); ++index) {
				Renderer::RenderNode &node = m_renderer->m_OpaqueRenderNodes[index];
				CBlock block = {};
				block.modelMat = node.globalTransform;
				block.camPos = vec4(cameraPos, 1.0);
				block.index[0] = node.material->m_BaseTexture->GetViewIndex();
				block.index[1] = block.index[2] = block.index[3] = 0u;
				block.testVec.y = 0.0;
				NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(CBlock));

				uint32_t instanceCount = 1;

				NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(node.drawArgs.indexNum), instanceCount, node.drawArgs.baseIndex, node.drawArgs.baseVertex, index });
			}
		} 
		// else {
		// 	nri::Buffer *indirectBuffer = m_renderer->gpuCullingPass->m_CullGPUSceneObjectsBuffer->GetBuffer();
		// 	NRI.CmdDrawIndexedIndirect(info.cmdBuffer, *indirectBuffer, 0, (uint32_t)m_renderer->m_OpaqueRenderNodes.size(), sizeof(nri::DrawIndexedDesc), nullptr, 0);
		// }
	}
}

void CommonMeshPass::RenderShadow(struct RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;
	// {
	// 	helper::Annotation annotation(NRI, info.cmdBuffer, "Shadow Pass");
	// 	NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_ShadowPipelineLayout);
	// 	NRI.CmdSetPipeline(info.cmdBuffer, *m_ShadowPipeline);
	// 	nri::ClearDesc clearDesc = {};
	// 	clearDesc.planes = nri::PlaneBits::DEPTH;
	// 	clearDesc.value.depthStencil.depth = 1.0;
	// 	NRI.CmdClearAttachments(info.cmdBuffer, &clearDesc, 1, nullptr, 0);
	// 	{
	// 		const nri::Viewport viewport = { 0.0f, 0.0f, 2048.f,
	// 			2048.f, 0.0f, 1.0f };
	// 		NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

	// 		nri::Rect scissor = { 0, 0, 2048, 2048 };
	// 		NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
	// 	}

	// 	nri::Buffer *geoBuffer = m_renderer->m_OpaqueRenderNodes[0].meshGPU->m_vertexbuffer->GetBuffer();
	// 	nri::VertexBufferDesc vertexBufferDesc = {};
	// 	vertexBufferDesc.buffer = geoBuffer;
	// 	vertexBufferDesc.stride = sizeof(utils::Vertex);
	// 	NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
	// 	nri::Buffer *indexGeoBuffer = m_renderer->m_OpaqueRenderNodes[0].meshGPU->m_indexbuffer->GetBuffer();
	// 	NRI.CmdSetIndexBuffer(info.cmdBuffer, *indexGeoBuffer, 0,
	// 			nri::IndexType::UINT32);

	// 	for (uint32_t index = 0; index < m_renderer->m_OpaqueRenderNodes.size(); ++index) {
	// 		Renderer::RenderNode &node = m_renderer->m_OpaqueRenderNodes[index];
	// 		CBlock block = {};
	// 		block.modelMat = node.globalTransform;
	// 		// block.modelMat = glm::scale(block.modelMat, glm::vec3(0.01f));
	// 		block.camPos = vec4(cameraPos, 1.0);
	// 		block.index[0] = node.material->m_BaseTexture->GetViewIndex();
	// 		block.index[1] = block.index[2] = block.index[3] = 1u;
	// 		block.testVec.y = 2.0f;
	// 		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(CBlock));

	// 		uint32_t instanceCount = 1;
	// 		NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(node.drawArgs.indexNum), instanceCount, node.drawArgs.baseIndex, node.drawArgs.baseVertex, index });
	// 	}
	// }

	const glm::mat4 p = camera.statePrev.mViewToClip;
	const glm::vec3 cameraPos = camera.statePrev.position;

	ConstantBufferLayout *commonConstants = (ConstantBufferLayout *)NRI.MapBuffer(
			*m_ConstantBuffer, 0,
			sizeof(ConstantBufferLayout));
	if (commonConstants) {
		commonConstants->modelMat = glm::mat4(1.0);
		commonConstants->viewMat = camera.state.mWorldToView;
		commonConstants->projectMat = camera.state.mViewToClip;
		commonConstants->lightVP = m_renderer->m_lightVP;
		NRI.UnmapBuffer(*m_ConstantBuffer);
	}

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Shadow Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_ShadowPipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_ShadowPipeline);
		nri::ClearDesc clearDesc = {};
		clearDesc.planes = nri::PlaneBits::DEPTH;
		clearDesc.value.depthStencil.depth = 1.0;
		NRI.CmdClearAttachments(info.cmdBuffer, &clearDesc, 1, nullptr, 0);
		{
			const nri::Viewport viewport = { 0.0f, 0.0f, 2048.f,
				2048.f, 0.0f, 1.0f };
			NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

			nri::Rect scissor = { 0, 0, 2048, 2048 };
			NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
		}

		nri::Buffer *geoBuffer = m_renderer->m_OpaqueRenderNodes[0].meshGPU->m_vertexbuffer->GetBuffer();
		nri::VertexBufferDesc vertexBufferDesc = {};
		vertexBufferDesc.buffer = geoBuffer;
		vertexBufferDesc.stride = sizeof(utils::Vertex);
		NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
		nri::Buffer *indexGeoBuffer = m_renderer->m_OpaqueRenderNodes[0].meshGPU->m_indexbuffer->GetBuffer();
		NRI.CmdSetIndexBuffer(info.cmdBuffer, *indexGeoBuffer, 0,
				nri::IndexType::UINT32);
		NRI.CmdSetDescriptorSet(info.cmdBuffer, 0,
				*m_ConstantBufferDescriptorSet, nullptr);
		if (!m_renderer->m_config.IndirectDrawState) {
			for (uint32_t index = 0; index < m_renderer->m_OpaqueRenderNodes.size(); ++index) {
				Renderer::RenderNode &node = m_renderer->m_OpaqueRenderNodes[index];
				CBlock block = {};
				block.modelMat = node.globalTransform;
				block.camPos = vec4(cameraPos, 1.0);
				block.index[0] = node.material->m_BaseTexture->GetViewIndex();
				block.index[1] = block.index[2] = block.index[3] = 1u;
				block.testVec.y = 2.0f;
				NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(CBlock));

				uint32_t instanceCount = 1;

				NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(node.drawArgs.indexNum), instanceCount, node.drawArgs.baseIndex, node.drawArgs.baseVertex, index });
			}
		} else {
			nri::Buffer *indirectBuffer = m_renderer->gpuCullingPass->m_CullGPUSceneObjectsBuffer->GetBuffer();
			NRI.CmdDrawIndexedIndirect(info.cmdBuffer, *indirectBuffer, 0, (uint32_t)m_renderer->m_OpaqueRenderNodes.size(), sizeof(nri::DrawIndexedDesc), nullptr, 0);
		}
	}
}