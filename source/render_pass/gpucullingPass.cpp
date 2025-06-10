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

		m_CullDataBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_CullDataBuffer->GetBuffer(), "CullDataBuffer");
	}

	{
		m_GPUSceneObjectsBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(nri::DrawIndexedDesc) * m_renderer->m_OpaqueRenderNodes.size();
		bufferDesc.usage = nri::BufferUsageBits::ARGUMENT_BUFFER | nri::BufferUsageBits::SHADER_RESOURCE;

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		viewDesc.format = nri::Format::R32_UINT;
        viewDesc.size = bufferDesc.size;

		m_GPUSceneObjectsBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_GPUSceneObjectsBuffer->GetBuffer(), "m_GPUSceneObjectsBuffer");
	}

	{
		std::vector<nri::DrawIndexedDesc> indirectBufferData;
		std::vector<CullData> cullDatas;

		indirectBufferData.resize(m_renderer->m_OpaqueRenderNodes.size());
		cullDatas.resize(m_renderer->m_OpaqueRenderNodes.size());

		for (size_t i = 0; i < m_renderer->m_OpaqueRenderNodes.size(); ++i) {
			Renderer::RenderNode node = m_renderer->m_OpaqueRenderNodes[i];
			indirectBufferData[i].indexNum = node.drawArgs.indexNum;
			indirectBufferData[i].instanceNum = 1;
			indirectBufferData[i].baseIndex = node.drawArgs.baseIndex;
			indirectBufferData[i].baseVertex = node.drawArgs.baseVertex;
			indirectBufferData[i].baseInstance = (uint32_t)i;

			cullDatas[i].center = node.mesh->aabb2.first;
			cullDatas[i].radians = std::max(node.mesh->aabb2.second.x, std::max(node.mesh->aabb2.second.y, node.mesh->aabb2.second.z));
		}

		nri::BufferUploadDesc bufferData = {};
		bufferData.buffer = m_GPUSceneObjectsBuffer->GetBuffer();
		bufferData.data = indirectBufferData.data();
		bufferData.dataSize = sizeof(nri::DrawIndexedDesc) * indirectBufferData.size();
		bufferData.after = { .access = nri::AccessBits::ARGUMENT_BUFFER, .stages = nri::StageBits::INDIRECT };

		nri::BufferUploadDesc bufferData3 = {};
		bufferData3.buffer = m_CullDataBuffer->GetBuffer();
		bufferData3.data = cullDatas.data();
		bufferData3.dataSize = sizeof(CullData) * cullDatas.size();
		bufferData3.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::COMPUTE_SHADER };

		std::vector<nri::BufferUploadDesc> uploadDescArray = { bufferData, bufferData3 };

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
		nri::DescriptorRangeDesc descriptorRangeTexture[1] = {};
		descriptorRangeTexture[0] = { 0, 2, nri::DescriptorType::STRUCTURED_BUFFER,
			nri::StageBits::COMPUTE_SHADER };
		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeTexture, 1 },
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
        nri::DescriptorRangeDesc descriptorRangeTexture = {};
        descriptorRangeTexture.descriptorNum = 2;
        descriptorRangeTexture.descriptorType = nri::DescriptorType::STRUCTURED_BUFFER;
        descriptorRangeTexture.shaderStages = nri::StageBits::COMPUTE_SHADER;

        NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_CullingPipelineLayout, 0,
				&m_CullingDescriptorSet, 1, 0));
		NRI.SetDebugName(m_CullingDescriptorSet, "m_CullingDescriptorSet");

        nri::Descriptor *cullDataBufferView = m_CullDataBuffer->GetView();
        nri::Descriptor *gpuSceneObjectsBufferView = m_GPUSceneObjectsBuffer->GetView();
        std::vector<nri::Descriptor *> descriptors = { cullDataBufferView, gpuSceneObjectsBufferView };
        nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[1] = {};
        descriptorRangeUpdateDescs[0].descriptorNum = (uint32_t)descriptors.size();
        descriptorRangeUpdateDescs[0].descriptors = descriptors.data();

        NRI.UpdateDescriptorRanges(*m_CullingDescriptorSet, 0,
				helper::GetCountOf(descriptorRangeUpdateDescs),
				descriptorRangeUpdateDescs);
    }
}

void GPUCullingPass::Render(struct RenderInfo &info, Camera &camera) {
}
