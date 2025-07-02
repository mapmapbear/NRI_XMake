#include "gpucullingPass.h"
#include "../renderer.h"
#include "Camera.h"
#include "buffer.h"
#include "glm/fwd.hpp"
#include "render_pass/commonRenderPass.h"
#include "texture.h"

GPUCullingPass::GPUCullingPass(Renderer *renderer) :
		CommonRenderPass(renderer) {
	m_NRI = &renderer->GetNRI();
	AllocGPUMemory();
	BindMemory();
	BuildPipeline();
}

void GPUCullingPass::AllocGPUMemory() {
	auto NRI = *m_NRI;

	{
		const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());
		const uint32_t constantBufferSize = helper::Align((uint32_t)sizeof(ConstantBufferLayout),
				deviceDesc.memoryAlignment.constantBufferOffset);

		{
			nri::BufferDesc bufferDesc = {};
			bufferDesc.size = constantBufferSize * BUFFERED_FRAME_MAX_NUM;
			bufferDesc.usage = nri::BufferUsageBits::CONSTANT_BUFFER;
			NRI_ABORT_ON_FAILURE(
					NRI.CreateBuffer(*m_renderer->GetRenderDevice(), bufferDesc, m_ConstantBuffer));
		}

		std::vector<nri::Buffer *> constantBufferArray = { m_ConstantBuffer };

		nri::ResourceGroupDesc resourceGroupDesc = {};
		resourceGroupDesc.memoryLocation = nri::MemoryLocation::HOST_UPLOAD;
		resourceGroupDesc.bufferNum = 1;
		resourceGroupDesc.buffers = &m_ConstantBuffer;

		m_MemoryAllocations.resize(1, nullptr);
		NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(*m_renderer->GetRenderDevice(), resourceGroupDesc,
				m_MemoryAllocations.data()));

		{
			nri::BufferViewDesc bufferViewDesc = {};
			bufferViewDesc.buffer = m_ConstantBuffer;
			bufferViewDesc.viewType = nri::BufferViewType::CONSTANT;
			bufferViewDesc.offset = 0;
			bufferViewDesc.size = constantBufferSize;
			NRI_ABORT_ON_FAILURE(
					NRI.CreateBufferView(bufferViewDesc, m_ConstantBufferView));
		}
	}

	// Culling GPU Resources
	{
		m_CullDataBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		uint32_t dataSize = sizeof(CullData);
		bufferDesc.size = dataSize * m_renderer->m_OpaqueRenderNodes.size();
		bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
		bufferDesc.structureStride = dataSize;

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		viewDesc.size = bufferDesc.size;
		viewDesc.structureStride = bufferDesc.structureStride;

		m_CullDataBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_CullDataBuffer->GetBuffer(), "CullDataBuffer");
	}

	{
		m_GPUSceneObjectsBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(nri::DrawIndexedDesc) * m_renderer->m_OpaqueRenderNodes.size();
		bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
		bufferDesc.structureStride = sizeof(nri::DrawIndexedDesc);

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		// viewDesc.format = nri::Format::R32_UINT;
		viewDesc.size = bufferDesc.size;
		viewDesc.structureStride = bufferDesc.structureStride;

		m_GPUSceneObjectsBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_GPUSceneObjectsBuffer->GetBuffer(), "m_GPUSceneObjectsBuffer");
	}

	{
		m_CullGPUSceneObjectsBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(nri::DrawIndexedDesc) * m_renderer->m_OpaqueRenderNodes.size();
		bufferDesc.usage = nri::BufferUsageBits::ARGUMENT_BUFFER | nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE_STORAGE;
		viewDesc.size = bufferDesc.size;
		viewDesc.structureStride = sizeof(nri::DrawIndexedDesc);

		m_CullGPUSceneObjectsBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_CullGPUSceneObjectsBuffer->GetBuffer(), "m_CullGPUSceneObjectsBuffer");
	}

	{
		m_VisibleObjectCounterBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(uint32_t);
		bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;
		bufferDesc.structureStride = sizeof(uint32_t);

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE_STORAGE;
		viewDesc.size = bufferDesc.size;
		viewDesc.structureStride = bufferDesc.structureStride;
		viewDesc.format = nri::Format::R32_UINT;

		m_VisibleObjectCounterBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_VisibleObjectCounterBuffer->GetBuffer(), "m_VisibleObjectCounterBuffer");
	}

	{
		m_VisibleFlagsBuffer = std::make_shared<Buffer>();
		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(uint32_t) * m_renderer->m_OpaqueRenderNodes.size();
		bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;
		bufferDesc.structureStride = sizeof(uint32_t);

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE_STORAGE;
		viewDesc.size = bufferDesc.size;
		viewDesc.structureStride = bufferDesc.structureStride;
		viewDesc.format = nri::Format::R32_UINT;

		m_VisibleFlagsBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_VisibleFlagsBuffer->GetBuffer(), "m_VisibleFlagsBuffer");
	}

	{
		std::vector<nri::DrawIndexedDesc> indirectBufferData;
		std::vector<nri::DrawIndexedDesc> cullIndirectBufferData;
		std::vector<CullData> cullDatas;

		indirectBufferData.resize(m_renderer->m_OpaqueRenderNodes.size());
		cullIndirectBufferData.resize(m_renderer->m_OpaqueRenderNodes.size());
		cullDatas.resize(m_renderer->m_OpaqueRenderNodes.size());

		for (size_t i = 0; i < m_renderer->m_OpaqueRenderNodes.size(); ++i) {
			Renderer::RenderNode node = m_renderer->m_OpaqueRenderNodes[i];
			indirectBufferData[i].indexNum = node.drawArgs.indexNum;
			indirectBufferData[i].instanceNum = 1;
			indirectBufferData[i].baseIndex = node.drawArgs.baseIndex;
			indirectBufferData[i].baseVertex = node.drawArgs.baseVertex;
			indirectBufferData[i].baseInstance = (uint32_t)i;

			glm::mat4 transMat = node.globalTransform;
			glm::vec3 min = glm::vec4(node.mesh->aabb.first, 1.0);
			glm::vec3 max = glm::vec4(node.mesh->aabb.second, 1.0);
			min = transMat * glm::vec4(min, 1.0);
			max = transMat * glm::vec4(max, 1.0);

			glm::vec3 center = (min + max) * 0.5f;
			glm::vec3 extent = (max - min) * 0.5f;

			cullDatas[i].center = center;
			cullDatas[i].extents = extent;
		}

		nri::BufferUploadDesc bufferData = {};
		bufferData.buffer = m_GPUSceneObjectsBuffer->GetBuffer();
		bufferData.data = indirectBufferData.data();
		// bufferData.dataSize = sizeof(nri::DrawIndexedDesc) * indirectBufferData.size();
		bufferData.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::COMPUTE_SHADER };

		nri::BufferUploadDesc bufferData2 = {};
		bufferData2.buffer = m_CullGPUSceneObjectsBuffer->GetBuffer();
		bufferData2.data = cullIndirectBufferData.data();
		// bufferData2.dataSize = sizeof(nri::DrawIndexedDesc) * cullIndirectBufferData.size();
		bufferData2.after = { .access = nri::AccessBits::SHADER_RESOURCE_STORAGE, .stages = nri::StageBits::COMPUTE_SHADER };

		nri::BufferUploadDesc bufferData3 = {};
		bufferData3.buffer = m_CullDataBuffer->GetBuffer();
		bufferData3.data = cullDatas.data();
		// bufferData3.dataSize = sizeof(CullData) * cullDatas.size();
		bufferData3.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::COMPUTE_SHADER };

		uint32_t visibleObjectCounter = 0;
		nri::BufferUploadDesc bufferData4 = {};
		bufferData4.buffer = m_VisibleObjectCounterBuffer->GetBuffer();
		bufferData4.data = &visibleObjectCounter;
		// bufferData4.dataSize = sizeof(uint32_t);
		bufferData4.after = { .access = nri::AccessBits::SHADER_RESOURCE_STORAGE, .stages = nri::StageBits::COMPUTE_SHADER };

		std::vector<uint32_t> visibleFlags(m_renderer->m_OpaqueRenderNodes.size(), 0);
		nri::BufferUploadDesc bufferData5 = {};
		bufferData5.buffer = m_VisibleFlagsBuffer->GetBuffer();
		bufferData5.data = visibleFlags.data();
		bufferData5.after = { .access = nri::AccessBits::SHADER_RESOURCE_STORAGE, .stages = nri::StageBits::COMPUTE_SHADER };

		std::vector<nri::BufferUploadDesc> uploadDescArray = { bufferData, bufferData2, bufferData3, bufferData4 };

		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), nullptr, 0,
				uploadDescArray.data(),
				(uint32_t)uploadDescArray.size()));
	}

	// HiZ GPU Resources
	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE_STORAGE | nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = nri::Format::R32_SFLOAT;
		textureDesc.width = m_renderer->m_OutputResolution.first / 2;
		textureDesc.height = m_renderer->m_OutputResolution.second / 2;
		textureDesc.mipNum = 10;

		nri::Texture2DViewDesc texture2DViewDesc = {};
		texture2DViewDesc.format = textureDesc.format;
		texture2DViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_STORAGE_2D;

		m_HiZTexture = std::make_shared<Texture>();
		m_HiZTexture->Create(m_renderer, textureDesc, texture2DViewDesc);
		NRI.SetDebugName(m_HiZTexture->GetTexture(), "m_HiZTexture");

		std::vector<std::vector<float>> data(textureDesc.mipNum);
		std::vector<nri::TextureSubresourceUploadDesc> subresources;
		
		for (uint32_t i = 0; i < textureDesc.mipNum; i++) {
			nri::TextureSubresourceUploadDesc subresource = {};
			uint32_t width = textureDesc.width >> i;
			uint32_t height = textureDesc.height >> i;
			subresource.rowPitch = width * sizeof(float);
			subresource.slicePitch = subresource.rowPitch * height;
			data[i].resize(width * height, 1.0f);
			subresource.slices = data[i].data();
			subresource.sliceNum = 1;
			subresources.push_back(subresource);
		}

		nri::TextureUploadDesc textureData = {};
		textureData.subresources = subresources.data();
		textureData.texture = m_HiZTexture->GetTexture();
		textureData.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE };
		textureData.planes = nri::PlaneBits::ALL;

		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), &textureData, 1, nullptr, 0));
	}
}

void GPUCullingPass::BindMemory() {
	auto NRI = *m_NRI;
	{
		nri::Texture2DViewDesc texture2DViewDes = { .texture = m_renderer->m_DepthTex, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = nri::Format::D32_SFLOAT };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDes, m_DepthTextureSRV));
	}

	{
		nri::Texture2DViewDesc texture2DViewDes = { .texture = m_HiZTexture->GetTexture(), .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = nri::Format::R32_SFLOAT };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDes, m_HiZTextureSRV));
	}

	{
		nri::SamplerDesc samplerDesc = {};
		samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE,
			nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
		samplerDesc.filters = { nri::Filter::NEAREST, nri::Filter::NEAREST,
			nri::Filter::NEAREST };
		// samplerDesc.anisotropy = 4;
		samplerDesc.mipMax = 3.0f;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateSampler(*m_renderer->GetRenderDevice(), samplerDesc, m_PointSampler));
		NRI.SetDebugName(m_PointSampler, "Point Sampler");
	}
}

void GPUCullingPass::BuildPipeline() {
	auto NRI = *m_NRI;

	// Pipeline
	{
		nri::DescriptorRangeDesc descriptorRangeTexture[2] = {};
		descriptorRangeTexture[0] = { 0, 2, nri::DescriptorType::STRUCTURED_BUFFER,
			nri::StageBits::COMPUTE_SHADER };
		descriptorRangeTexture[1] = { 1, 3, nri::DescriptorType::STORAGE_STRUCTURED_BUFFER,
			nri::StageBits::COMPUTE_SHADER };

		nri::DescriptorRangeDesc descriptorRangeConstantBuffer = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeTexture, helper::GetCountOf(descriptorRangeTexture) },
			{ 1, &descriptorRangeConstantBuffer, 1 },
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(PushConstants),
			nri::StageBits::COMPUTE_SHADER };

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.descriptorSetNum =
				helper::GetCountOf(descriptorSetDescs);
		pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
		pipelineLayoutDesc.rootConstants = &rootConstant;
		pipelineLayoutDesc.rootConstantNum = 1;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::COMPUTE_SHADER;
		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_CullingPipelineLayout));

		utils::ShaderCodeStorage shaderCodeStorage;
		nri::ComputePipelineDesc computePipelineDesc = {};
		computePipelineDesc.pipelineLayout = m_CullingPipelineLayout;
		computePipelineDesc.shader = utils::LoadShader(nri::GraphicsAPI::D3D12, "culling.cs_8AB7D080", shaderCodeStorage);
		NRI_ABORT_ON_FAILURE(NRI.CreateComputePipeline(*m_renderer->GetRenderDevice(), computePipelineDesc, m_CullingPipeline));
		computePipelineDesc.shader = utils::LoadShader(nri::GraphicsAPI::D3D12, "culling.cs_1952B1E7", shaderCodeStorage);
		NRI_ABORT_ON_FAILURE(NRI.CreateComputePipeline(*m_renderer->GetRenderDevice(), computePipelineDesc, m_CullingPipeline2));
	}

	// HiZ Pipeline
	{
		nri::DescriptorRangeDesc descriptorRangeTexture[3] = {};
		descriptorRangeTexture[0].descriptorNum = 2;
		descriptorRangeTexture[0].descriptorType = nri::DescriptorType::TEXTURE;
		descriptorRangeTexture[0].shaderStages = nri::StageBits::COMPUTE_SHADER;
		descriptorRangeTexture[1].descriptorNum = m_HiZTexture->GetMipNum();
		descriptorRangeTexture[1].descriptorType = nri::DescriptorType::STORAGE_TEXTURE;
		descriptorRangeTexture[1].shaderStages = nri::StageBits::COMPUTE_SHADER;
		descriptorRangeTexture[2].descriptorNum = 1;
		descriptorRangeTexture[2].descriptorType = nri::DescriptorType::SAMPLER;
		descriptorRangeTexture[2].shaderStages = nri::StageBits::COMPUTE_SHADER;

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeTexture, helper::GetCountOf(descriptorRangeTexture) },
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(HiZPushConstants),
			nri::StageBits::COMPUTE_SHADER };

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.descriptorSetNum =
				helper::GetCountOf(descriptorSetDescs);
		pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
		pipelineLayoutDesc.rootConstants = &rootConstant;
		pipelineLayoutDesc.rootConstantNum = 1;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::COMPUTE_SHADER;
		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_HiZPipelineLayout));

		utils::ShaderCodeStorage shaderCodeStorage;
		nri::ComputePipelineDesc computePipelineDesc = {};
		computePipelineDesc.pipelineLayout = m_HiZPipelineLayout;
		computePipelineDesc.shader = utils::LoadShader(nri::GraphicsAPI::D3D12, "hizBuild.cs", shaderCodeStorage);
		NRI_ABORT_ON_FAILURE(NRI.CreateComputePipeline(*m_renderer->GetRenderDevice(), computePipelineDesc, m_HiZPipeline));
	}

	// Update Culling Descriptor Set
	{
		NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_CullingPipelineLayout, 0,
				&m_CullingDescriptorSet, 1, 0));
		NRI.SetDebugName(m_CullingDescriptorSet, "m_CullingDescriptorSet");

		NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_CullingPipelineLayout, 1,
				&m_CullingDescriptorConstantBufferSet, 1, 0));
		NRI.SetDebugName(m_CullingDescriptorConstantBufferSet, "m_CullingDescriptorConstantBufferSet");

		nri::Descriptor *cullDataBufferView = m_CullDataBuffer->GetView();
		nri::Descriptor *gpuSceneObjectsBufferView = m_GPUSceneObjectsBuffer->GetView();
		std::vector<nri::Descriptor *> descriptors = { cullDataBufferView, gpuSceneObjectsBufferView };
		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = (uint32_t)descriptors.size();
		descriptorRangeUpdateDescs[0].descriptors = descriptors.data();

		std::vector<nri::Descriptor *> descriptors2 = { m_CullGPUSceneObjectsBuffer->GetView(), m_VisibleObjectCounterBuffer->GetView(), m_VisibleFlagsBuffer->GetView() };
		descriptorRangeUpdateDescs[1].descriptorNum = (uint32_t)descriptors2.size();
		descriptorRangeUpdateDescs[1].descriptors = descriptors2.data();

		NRI.UpdateDescriptorRanges(*m_CullingDescriptorSet, 0,
				helper::GetCountOf(descriptorRangeUpdateDescs),
				descriptorRangeUpdateDescs);

		nri::Descriptor *constantBufferView = m_ConstantBufferView;
		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs2[1] = {};
		descriptorRangeUpdateDescs2[0].descriptorNum = 1;
		descriptorRangeUpdateDescs2[0].descriptors = &constantBufferView;

		NRI.UpdateDescriptorRanges(*m_CullingDescriptorConstantBufferSet, 0,
				helper::GetCountOf(descriptorRangeUpdateDescs2),
				descriptorRangeUpdateDescs2);
	}

	// Update Hi-Z Descriptor Set
	{
		nri::DescriptorRangeDesc descriptorRangeTexture[3] = {};
		descriptorRangeTexture[0].descriptorNum = 2;
		descriptorRangeTexture[0].descriptorType = nri::DescriptorType::TEXTURE;
		descriptorRangeTexture[0].shaderStages = nri::StageBits::COMPUTE_SHADER;
		descriptorRangeTexture[1].descriptorNum = m_HiZTexture->GetMipNum();
		descriptorRangeTexture[1].descriptorType = nri::DescriptorType::STORAGE_TEXTURE;
		descriptorRangeTexture[1].shaderStages = nri::StageBits::COMPUTE_SHADER;
		descriptorRangeTexture[2].descriptorNum = 1;
		descriptorRangeTexture[2].descriptorType = nri::DescriptorType::SAMPLER;
		descriptorRangeTexture[2].shaderStages = nri::StageBits::COMPUTE_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_HiZPipelineLayout, 0,
				&m_HiZDescriptorSet, 1, 0));
		NRI.SetDebugName(m_HiZDescriptorSet, "m_HiZDescriptorSet");

		std::vector<nri::Descriptor *> hizStorageTextureViews;
		for (uint32_t i = 0; i < m_HiZTexture->GetMipNum(); i++) {
			hizStorageTextureViews.push_back(m_HiZTexture->GetView(i));
		}
		nri::Descriptor *hizSamplerView = m_PointSampler;
		std::vector<nri::Descriptor *> descriptors = { m_DepthTextureSRV, m_HiZTextureSRV };
		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[3] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = (uint32_t)descriptors.size();
		descriptorRangeUpdateDescs[0].descriptors = descriptors.data();
		descriptorRangeUpdateDescs[1].descriptorNum = m_HiZTexture->GetMipNum();
		descriptorRangeUpdateDescs[1].descriptors = hizStorageTextureViews.data();
		descriptorRangeUpdateDescs[2].descriptorNum = 1;
		descriptorRangeUpdateDescs[2].descriptors = &hizSamplerView;

		NRI.UpdateDescriptorRanges(*m_HiZDescriptorSet, 0,
				helper::GetCountOf(descriptorRangeUpdateDescs),
				descriptorRangeUpdateDescs);
	}
}

glm::vec4 normalizePlane(const glm::vec4 &plane) {
	float length = glm::length(plane);
	if (length > 0.0f) {
		return plane / length;
	}
	return plane;
}

void GPUCullingPass::Render(struct RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;

	ConstantBufferLayout *commonConstants = (ConstantBufferLayout *)NRI.MapBuffer(
			*m_ConstantBuffer, 0,
			sizeof(ConstantBufferLayout));
	const glm::mat4 p = camera.state.mViewToClip;
	if (commonConstants) {
		commonConstants->modelMat = glm::mat4(1.0);
		commonConstants->viewMat = camera.state.mWorldToView;
		commonConstants->projectMat = p;
		NRI.UnmapBuffer(*m_ConstantBuffer);
	}

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Frustum Culling Pre Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_CullingPipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_CullingPipeline);
		glm::mat4 projMat = camera.state.mViewToClip;

		glm::vec4 frustumL = normalizePlane(projMat[3] + projMat[0]);
		glm::vec4 frustumR = normalizePlane(projMat[3] - projMat[0]);
		glm::vec4 frustumT = normalizePlane(projMat[3] + projMat[1]);
		glm::vec4 frustumB = normalizePlane(projMat[3] - projMat[1]);

		PushConstants block = {
			.viewMat = p * camera.state.mWorldToView, // * glm::rotate(glm::mat4(1.0), glm::radians(180.f), glm::vec3(0.0f, 1.0f, 0.0f)),
			.cameraArgs = glm::vec4(camera.m_desc.nearZ, camera.m_desc.farZ, camera.m_desc.farZ + 20, 0.0f),
			.frustum = { glm::vec4(frustumL.x, frustumL.y, frustumL.z, frustumL.w),
					glm::vec4(frustumR.x, frustumR.y, frustumR.z, frustumR.w),
					glm::vec4(frustumT.x, frustumT.y, frustumT.z, frustumT.w),
					glm::vec4(frustumB.x, frustumB.y, frustumB.z, frustumB.w) },
			.totalObjectCount = (uint32_t)m_renderer->m_OpaqueRenderNodes.size(),

		};
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(PushConstants));
		NRI.CmdSetDescriptorSet(info.cmdBuffer, 1, *m_CullingDescriptorConstantBufferSet, nullptr);
		NRI.CmdDispatch(info.cmdBuffer, { (uint32_t)floor(m_renderer->m_OpaqueRenderNodes.size() / 32) + 1, 1, 1 });
	}
}

void GPUCullingPass::RenderPost(struct RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;

	ConstantBufferLayout *commonConstants = (ConstantBufferLayout *)NRI.MapBuffer(
			*m_ConstantBuffer, 0,
			sizeof(ConstantBufferLayout));
	const glm::mat4 p = camera.state.mViewToClip;
	if (commonConstants) {
		commonConstants->modelMat = glm::mat4(1.0);
		commonConstants->viewMat = camera.state.mWorldToView;
		commonConstants->projectMat = p;
		NRI.UnmapBuffer(*m_ConstantBuffer);
	}

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Frustum Culling Post Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_CullingPipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_CullingPipeline2);
		glm::mat4 projMat = camera.state.mViewToClip;

		glm::vec4 frustumL = normalizePlane(projMat[3] + projMat[0]);
		glm::vec4 frustumR = normalizePlane(projMat[3] - projMat[0]);
		glm::vec4 frustumT = normalizePlane(projMat[3] + projMat[1]);
		glm::vec4 frustumB = normalizePlane(projMat[3] - projMat[1]);

		PushConstants block = {
			.viewMat = p * camera.state.mWorldToView, // * glm::rotate(glm::mat4(1.0), glm::radians(180.f), glm::vec3(0.0f, 1.0f, 0.0f)),
			.cameraArgs = glm::vec4(camera.m_desc.nearZ, camera.m_desc.farZ, camera.m_desc.farZ + 20, 0.0f),
			.frustum = { glm::vec4(frustumL.x, frustumL.y, frustumL.z, frustumL.w),
					glm::vec4(frustumR.x, frustumR.y, frustumR.z, frustumR.w),
					glm::vec4(frustumT.x, frustumT.y, frustumT.z, frustumT.w),
					glm::vec4(frustumB.x, frustumB.y, frustumB.z, frustumB.w) },
			.totalObjectCount = (uint32_t)m_renderer->m_OpaqueRenderNodes.size(),

		};
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(PushConstants));
		NRI.CmdSetDescriptorSet(info.cmdBuffer, 1, *m_CullingDescriptorConstantBufferSet, nullptr);
		NRI.CmdDispatch(info.cmdBuffer, { (uint32_t)floor(m_renderer->m_OpaqueRenderNodes.size() / 32) + 1, 1, 1 });
	}
}

void GPUCullingPass::RenderHiZ(struct RenderInfo &info) {
	auto NRI = *m_NRI;
	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Hi-Z Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_HiZPipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_HiZPipeline);
		uint32_t srcWidth = m_renderer->m_OutputResolution.first / 2;
		uint32_t srcHeight = m_renderer->m_OutputResolution.second / 2;

		for (uint32_t i = 0; i < m_HiZTexture->GetMipNum(); i++) {
			{
				nri::TextureBarrierDesc textureBarrierDescs = {};
				textureBarrierDescs.texture = m_HiZTexture->GetTexture();
				textureBarrierDescs.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE,
					nri::Layout::SHADER_RESOURCE_STORAGE };
				nri::BarrierGroupDesc barrierGroupDesc = {};
				barrierGroupDesc.textureNum = 1;
				barrierGroupDesc.textures = &textureBarrierDescs;
				NRI.CmdBarrier(info.cmdBuffer, barrierGroupDesc);
			}

			uint32_t destWidth = std::max(1u, (uint32_t)(srcWidth >> i));
			uint32_t destHeight = std::max(1u, (uint32_t)(srcHeight >> i));
			HiZPushConstants block = {
				.DimensionsInv = 1.0f / glm::vec2(destWidth, destHeight),
				.texDepth = 1021,
				.texHiZ = 1023 + i,
				.sampleIndex = 5,
			};
			NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(HiZPushConstants));
			NRI.CmdDispatch(info.cmdBuffer, { destWidth / 8 + 1, destHeight / 8 + 1, 1 });

			{
				nri::TextureBarrierDesc textureBarrierDescs = {};
				textureBarrierDescs.texture = m_HiZTexture->GetTexture();
				textureBarrierDescs.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE,
					nri::Layout::SHADER_RESOURCE_STORAGE };
				textureBarrierDescs.after = { nri::AccessBits::SHADER_RESOURCE,
					nri::Layout::SHADER_RESOURCE };
				nri::BarrierGroupDesc barrierGroupDesc = {};
				barrierGroupDesc.textureNum = 1;
				barrierGroupDesc.textures = &textureBarrierDescs;
				NRI.CmdBarrier(info.cmdBuffer, barrierGroupDesc);
			}
		}
	}
}