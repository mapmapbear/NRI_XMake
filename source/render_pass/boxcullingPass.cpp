#include "boxcullingPass.h"
#include "buffer.h"
#include "renderer.h"
#include <memory>
#include <random>

#define INSTANCE_COUNT 50000

BoxCullingPass::BoxCullingPass(Renderer *renderer) :
		CommonRenderPass(renderer) {
	AllocGPUMemory();
	BindMemory();
	BuildPipeline();
}

void BoxCullingPass::AllocGPUMemory() {
}

void BoxCullingPass::BindMemory() {
	auto NRI = *m_NRI;

	const std::vector<float3> vertices = {
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.5f, 0.0f },
		{ 0.5f, 0.0f, 0.0f }
	};

	std::vector<float3> instancePositions(INSTANCE_COUNT);
	std::mt19937 gen(std::random_device{}());
	std::uniform_real_distribution<> distrib(-200, 200);
	for (uint32_t i = 0; i < INSTANCE_COUNT; ++i) {
		instancePositions[i] = { (float)distrib(gen), (float)distrib(gen), (float)distrib(gen) };
	}

	{
		m_vertexPosBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(float3) * vertices.size();
		bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;

		nri::BufferViewDesc viewDesc = {};

		m_vertexPosBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_vertexPosBuffer->GetBuffer(), "vertexPosBuffer");
	}

	{
		m_positionBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(float3) * instancePositions.size();
		bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
		bufferDesc.structureStride = sizeof(float3);

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		viewDesc.size = bufferDesc.size;
		m_positionBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_positionBuffer->GetBuffer(), "positionBuffer");
	}

	{
		m_indirectBuffer = std::make_shared<Buffer>();

		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = sizeof(nri::DrawDesc);
		bufferDesc.usage = nri::BufferUsageBits::ARGUMENT_BUFFER | nri::BufferUsageBits::SHADER_RESOURCE;

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		viewDesc.size = sizeof(nri::DrawDesc);
		viewDesc.format = nri::Format::R32_UINT;

		m_indirectBuffer->Create(m_renderer, bufferDesc, viewDesc);
		NRI.SetDebugName(m_indirectBuffer->GetBuffer(), "indirectBuffer");
	}

	// upload buffer
	{
		const uint64_t vertexDataSize = helper::GetByteSizeOf(vertices);
		std::vector<uint8_t> geometryBufferData(vertexDataSize);
		memcpy(geometryBufferData.data(), vertices.data(), vertexDataSize);

		nri::BufferUploadDesc bufferData = {};
		bufferData.buffer = m_vertexPosBuffer->GetBuffer();
		bufferData.data = &geometryBufferData[0];
		// bufferData.dataSize = geometryBufferData.size();
		bufferData.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::COMPUTE_SHADER };

		const uint64_t positionDataSize = helper::GetByteSizeOf(instancePositions);
		std::vector<uint8_t> positionBufferData(positionDataSize);
		memcpy(positionBufferData.data(), instancePositions.data(), positionDataSize);

		nri::BufferUploadDesc bufferData1 = {};
		bufferData1.buffer = m_positionBuffer->GetBuffer();
		bufferData1.data = positionBufferData.data();
		// bufferData1.dataSize = positionBufferData.size();
		bufferData1.after = { .access = nri::AccessBits::SHADER_RESOURCE, .stages = nri::StageBits::COMPUTE_SHADER };

		nri::DrawDesc indirectBufferData = {};
		indirectBufferData.vertexNum = 3;
		indirectBufferData.instanceNum = INSTANCE_COUNT;
		indirectBufferData.baseVertex = 0;
		indirectBufferData.baseInstance = 0;

		nri::BufferUploadDesc bufferData2 = {};
		bufferData2.buffer = m_indirectBuffer->GetBuffer();
		bufferData2.data = &indirectBufferData;
		// bufferData2.dataSize = sizeof(nri::DrawDesc);
		bufferData2.after = { .access = nri::AccessBits::ARGUMENT_BUFFER, .stages = nri::StageBits::INDIRECT };

		std::vector<nri::BufferUploadDesc> uploadDescArray = { bufferData, bufferData1, bufferData2 };

		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), nullptr, 0,
				uploadDescArray.data(),
				(uint32_t)uploadDescArray.size()));
	}
}

void BoxCullingPass::BuildPipeline() {
	auto NRI = *m_NRI;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());

	nri::DescriptorRangeDesc descriptorRangeConstant = { 0, 1, nri::DescriptorType::STRUCTURED_BUFFER };
	nri::DescriptorSetDesc descriptorSetDescs = { 0, &descriptorRangeConstant,
		1 };

	nri::RootConstantDesc rootConstant = { 1, sizeof(glm::mat4),
		nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER };

	nri::PipelineLayoutDesc pipelineLayoutDesc = {};
	pipelineLayoutDesc.rootConstantNum = 1;
	pipelineLayoutDesc.rootConstants = &rootConstant;
	pipelineLayoutDesc.descriptorSetNum = 1;
	pipelineLayoutDesc.descriptorSets = &descriptorSetDescs;
	pipelineLayoutDesc.shaderStages =
			nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

	NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
			m_PipelineLayout));

	nri::VertexStreamDesc vertexStreamDesc = {};
	vertexStreamDesc.bindingSlot = 0;

	nri::VertexAttributeDesc vertexAttributeDesc[1] = {};
	{
		vertexAttributeDesc[0].format = nri::Format::RGB32_SFLOAT;
		vertexAttributeDesc[0].streamIndex = 0;
		vertexAttributeDesc[0].offset = helper::GetOffsetOf(&utils::Vertex::position);
		vertexAttributeDesc[0].d3d = { "POSITION", 0 };
		vertexAttributeDesc[0].vk.location = { 0 };
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
	rasterizationDesc.frontCounterClockwise = true;

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
	depthAttachmentDesc.compareFunc = nri::CompareFunc::GREATER_EQUAL;
	depthAttachmentDesc.boundsTest = false;

	nri::OutputMergerDesc outputMergerDesc = {};
	outputMergerDesc.colors = &colorAttachmentDesc;
	outputMergerDesc.colorNum = 1;
	outputMergerDesc.depth = depthAttachmentDesc;
	outputMergerDesc.depthStencilFormat = nri::Format::D32_SFLOAT;

	utils::ShaderCodeStorage shaderCodeStorage;
	nri::ShaderDesc shaderStages[] = {
		utils::LoadShader(deviceDesc.graphicsAPI,
				"triangle", shaderCodeStorage, "vs_main"),
		utils::LoadShader(deviceDesc.graphicsAPI, "triangle",
				shaderCodeStorage, "ps_main"),
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

	{
		NRI_ABORT_ON_FAILURE(
				NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_PipelineLayout, 0,
						&m_DescriptorSet, 1, 0));

		nri::Descriptor *view = m_positionBuffer->GetView();
		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs = {};
		descriptorRangeUpdateDescs.descriptorNum = 1;
		descriptorRangeUpdateDescs.descriptors = &view;
		NRI.UpdateDescriptorRanges(*m_DescriptorSet, 0, 1, &descriptorRangeUpdateDescs);
	}
}

void BoxCullingPass::Render(struct RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Indirect Triangle Draw");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		{
			const nri::Viewport viewport = { 0.0f, 0.0f, (float)m_renderer->m_OutputResolution.first,
				(float)m_renderer->m_OutputResolution.second, 0.0f, 1.0f };
			NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

			nri::Rect scissor = { 0, 0, (uint16_t)m_renderer->m_OutputResolution.first, (uint16_t)m_renderer->m_OutputResolution.second };
			NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);	
		}
		glm::mat4 vpMat = camera.state.mViewToClip * camera.state.mWorldToView;
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &vpMat, sizeof(glm::mat4));

		nri::VertexBufferDesc vertexBufferDesc = {};
		vertexBufferDesc.buffer = m_vertexPosBuffer->GetBuffer();
		vertexBufferDesc.offset = 0;
		vertexBufferDesc.stride = sizeof(glm::vec3);
		NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
		if (m_renderer->m_config.IndirectDrawState) {
			NRI.CmdDrawIndirect(info.cmdBuffer, *m_indirectBuffer->GetBuffer(), 0, 1, sizeof(nri::DrawBaseDesc), nullptr, 0);
		} else {
			NRI.CmdDraw(info.cmdBuffer, { 3, INSTANCE_COUNT, 0, 0 });
		}
	}
}
