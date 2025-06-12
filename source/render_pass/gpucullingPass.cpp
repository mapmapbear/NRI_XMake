#include "gpucullingPass.h"
#include "../renderer.h"
#include "buffer.h"
#include "render_pass/commonRenderPass.h"

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
		// viewDesc.format = nri::Format::R32_UINT;
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

		m_VisibleObjectCounterBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_VisibleObjectCounterBuffer->GetBuffer(), "m_VisibleObjectCounterBuffer");
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
			glm::vec4 center = glm::vec4(node.mesh->aabb2.first, 1.0);
			center = transMat * center;
			glm::vec4 extent = glm::vec4(node.mesh->aabb2.second, 1.0);
			extent = transMat * extent;
			cullDatas[i].center = center;
			cullDatas[i].radians = std::min(extent.x, std::min(extent.y, extent.z));
		}

		nri::BufferUploadDesc bufferData = {};
		bufferData.buffer = m_GPUSceneObjectsBuffer->GetBuffer();
		bufferData.data = indirectBufferData.data();
		bufferData.dataSize = sizeof(nri::DrawIndexedDesc) * indirectBufferData.size();
		bufferData.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::COMPUTE_SHADER };

		nri::BufferUploadDesc bufferData2 = {};
		bufferData2.buffer = m_CullGPUSceneObjectsBuffer->GetBuffer();
		bufferData2.data = cullIndirectBufferData.data();
		bufferData2.dataSize = sizeof(nri::DrawIndexedDesc) * cullIndirectBufferData.size();
		bufferData2.after = { .access = nri::AccessBits::SHADER_RESOURCE_STORAGE, .stages = nri::StageBits::COMPUTE_SHADER };

		nri::BufferUploadDesc bufferData3 = {};
		bufferData3.buffer = m_CullDataBuffer->GetBuffer();
		bufferData3.data = cullDatas.data();
		bufferData3.dataSize = sizeof(CullData) * cullDatas.size();
		bufferData3.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::COMPUTE_SHADER };

		uint32_t visibleObjectCounter = 0;
		nri::BufferUploadDesc bufferData4 = {};
		bufferData4.buffer = m_VisibleObjectCounterBuffer->GetBuffer();
		bufferData4.data = &visibleObjectCounter;
		bufferData4.dataSize = sizeof(uint32_t);
		bufferData4.after = { .access = nri::AccessBits::SHADER_RESOURCE_STORAGE, .stages = nri::StageBits::COMPUTE_SHADER };

		std::vector<nri::BufferUploadDesc> uploadDescArray = { bufferData, bufferData2, bufferData3, bufferData4 };

		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), nullptr, 0,
				uploadDescArray.data(),
				(uint32_t)uploadDescArray.size()));
	}
}

void GPUCullingPass::BindMemory() {
}

void GPUCullingPass::BuildPipeline() {
	auto NRI = *m_NRI;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());

	// Pipeline
	{
		nri::DescriptorRangeDesc descriptorRangeTexture[2] = {};
		descriptorRangeTexture[0] = { 0, 2, nri::DescriptorType::STRUCTURED_BUFFER,
			nri::StageBits::COMPUTE_SHADER };
		descriptorRangeTexture[1] = { 1, 2, nri::DescriptorType::STORAGE_STRUCTURED_BUFFER,
			nri::StageBits::COMPUTE_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeTexture, 2 },
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
		computePipelineDesc.shader = utils::LoadShader(nri::GraphicsAPI::D3D12, "culling.cs", shaderCodeStorage);
		NRI_ABORT_ON_FAILURE(NRI.CreateComputePipeline(*m_renderer->GetRenderDevice(), computePipelineDesc, m_CullingPipeline));
	}

	{
		nri::DescriptorRangeDesc descriptorRangeTexture[2] = {};
		descriptorRangeTexture[0].descriptorNum = 2;
		descriptorRangeTexture[0].descriptorType = nri::DescriptorType::STRUCTURED_BUFFER;
		descriptorRangeTexture[0].shaderStages = nri::StageBits::COMPUTE_SHADER;

		descriptorRangeTexture[1].descriptorNum = 2;
		descriptorRangeTexture[1].descriptorType = nri::DescriptorType::STRUCTURED_BUFFER;
		descriptorRangeTexture[1].shaderStages = nri::StageBits::COMPUTE_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_CullingPipelineLayout, 0,
				&m_CullingDescriptorSet, 1, 0));
		NRI.SetDebugName(m_CullingDescriptorSet, "m_CullingDescriptorSet");

		nri::Descriptor *cullDataBufferView = m_CullDataBuffer->GetView();
		nri::Descriptor *gpuSceneObjectsBufferView = m_GPUSceneObjectsBuffer->GetView();
		std::vector<nri::Descriptor *> descriptors = { cullDataBufferView, gpuSceneObjectsBufferView };
		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = (uint32_t)descriptors.size();
		descriptorRangeUpdateDescs[0].descriptors = descriptors.data();

		std::vector<nri::Descriptor *> descriptors2 = { m_CullGPUSceneObjectsBuffer->GetView(), m_VisibleObjectCounterBuffer->GetView() };
		descriptorRangeUpdateDescs[1].descriptorNum = (uint32_t)descriptors2.size();
		descriptorRangeUpdateDescs[1].descriptors = descriptors2.data();

		NRI.UpdateDescriptorRanges(*m_CullingDescriptorSet, 0,
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
	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Frustum Culling Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_CullingPipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_CullingPipeline);
		glm::mat4 projMat = camera.state.mViewToClip;
		// projMat = glm::transpose(projMat);
		glm::vec4 frustumX1 = normalizePlane(projMat[3] + projMat[0]);
		glm::vec4 frustumX2 = normalizePlane(projMat[3] - projMat[0]);
		glm::vec4 frustumY1 = normalizePlane(projMat[3] + projMat[1]);
		glm::vec4 frustumY2 = normalizePlane(projMat[3] - projMat[1]);

		PushConstants block = {
			.viewMat = camera.state.mWorldToView,
			.cameraArgs = glm::vec4(camera.m_desc.nearZ, camera.m_desc.farZ, camera.m_desc.farZ + 20, 0.0f),
			.frustum = { glm::vec4(frustumX1.x, frustumX1.y, frustumX1.z, frustumX1.w),
				glm::vec4(frustumX2.x, frustumX2.y, frustumX2.z, frustumX2.w),
				glm::vec4(frustumY1.x, frustumY1.y, frustumY1.z, frustumY1.w),
				glm::vec4(frustumY2.x, frustumY2.y, frustumY2.z, frustumY2.w) },
			.totalObjectCount = (uint32_t)m_renderer->m_OpaqueRenderNodes.size(),

		};
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(PushConstants));
		NRI.CmdDispatch(info.cmdBuffer, { (uint32_t)m_renderer->m_OpaqueRenderNodes.size() / 8 + 1, 1, 1 });
	}
}