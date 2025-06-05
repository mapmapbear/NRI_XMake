#include "debugDrawPass.h"
#include "NRIDescs.h"
#include "buffer.h"
#include "glm/ext/matrix_transform.hpp"
#include "renderer.h"
#include <cstdint>
#include <memory>

struct CBlock {
	glm::mat4 modelMat;
	glm::vec4 camPos;
	glm::vec4 testVec;
	glm::vec4 baseColor;
	glm::vec4 pbrParams;
	uint32_t index[4];
};

DebugDrawPass::DebugDrawPass(Renderer *renderer) :
		CommonRenderPass(renderer) {
	AllocGPUMemory();
	BindMemory();
	BuildPipeline();
}

void DebugDrawPass::AllocGPUMemory() {
	auto NRI = *m_NRI;

	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());
	const uint32_t constantBufferSize = helper::Align((uint32_t)sizeof(ConstantBufferLayout),
			deviceDesc.memoryAlignment.constantBufferOffset);

	// {
	// 	nri::BufferDesc bufferDesc = {};
	// 	bufferDesc.size = constantBufferSize * BUFFERED_FRAME_MAX_NUM;
	// 	bufferDesc.usage = nri::BufferUsageBits::CONSTANT_BUFFER;
	// 	NRI_ABORT_ON_FAILURE(
	// 			NRI.CreateBuffer(*m_renderer->GetRenderDevice(), bufferDesc, m_ConstantBuffer));
	// }

	// std::vector<nri::Buffer *> constantBufferArray = { m_ConstantBuffer };

	// nri::ResourceGroupDesc resourceGroupDesc = {};
	// resourceGroupDesc.memoryLocation = nri::MemoryLocation::HOST_UPLOAD;
	// resourceGroupDesc.bufferNum = 1;
	// resourceGroupDesc.buffers = &m_ConstantBuffer;

	// m_MemoryAllocations.resize(1, nullptr);
	// NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(*m_renderer->GetRenderDevice(), resourceGroupDesc,
	// 		m_MemoryAllocations.data()));
}

void DebugDrawPass::BindMemory() {
	auto NRI = *m_NRI;
	float minX = -1.0f;
	float minY = -1.0f;
	float minZ = -1.0f;
	float maxX = 1.0f;
	float maxY = 1.0f;
	float maxZ = 1.0f;
	glm::vec4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
	m_positions = {
		{ { minX, minY, minZ }, { minX, minY, minZ, 1.0 } }, // color }, // 0
		{ { maxX, minY, minZ }, { maxX, minY, minZ, 1.0 } }, // color }, // 1
		{ { maxX, maxY, minZ }, { maxX, maxY, minZ, 1.0 } }, // color }, // 2
		{ { minX, maxY, minZ }, { minX, maxY, minZ, 1.0 } }, // color }, // 3
		{ { minX, minY, maxZ }, { minX, minY, maxZ, 1.0 } }, // color }, // 4
		{ { maxX, minY, maxZ }, { maxX, minY, maxZ, 1.0 } }, // color }, // 5
		{ { maxX, maxY, maxZ }, { maxX, maxY, maxZ, 1.0 } }, // color }, // 6
		{ { minX, maxY, maxZ }, { minX, maxY, maxZ, 1.0 } } // color } // 7
	};

	m_indices = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	m_IndexCount = (uint32_t)m_indices.size();
	const uint64_t indexDataSize = helper::GetByteSizeOf(m_indices);
	const uint64_t indexDataAlignedSize = helper::Align(indexDataSize, 32);
	const uint64_t vertexDataSize = helper::GetByteSizeOf(m_positions);

	{
		m_GeometryBuffer = std::make_shared<Buffer>();
		nri::BufferDesc desc = {};
		desc.size = indexDataAlignedSize + vertexDataSize;
		desc.usage = nri::BufferUsageBits::VERTEX_BUFFER | nri::BufferUsageBits::INDEX_BUFFER;

		nri::BufferViewDesc viewDesc = {};
		m_GeometryBuffer->Create(m_renderer, desc, viewDesc);
	}

	m_indicesOffset = indexDataAlignedSize;
	std::vector<uint8_t> geometryBufferData(indexDataAlignedSize +
			vertexDataSize);
	memcpy(&geometryBufferData[0], m_indices.data(), indexDataSize);
	memcpy(&geometryBufferData[indexDataAlignedSize], m_positions.data(),
			vertexDataSize);

	nri::BufferUploadDesc bufferData = {};
	bufferData.buffer = m_GeometryBuffer->GetBuffer();
	bufferData.data = &geometryBufferData[0];
	bufferData.dataSize = geometryBufferData.size();
	bufferData.after = { nri::AccessBits::INDEX_BUFFER |
		nri::AccessBits::VERTEX_BUFFER };

	NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), nullptr, (uint32_t)0,
			&bufferData, 1));
}

void DebugDrawPass::BuildPipeline() {
	auto NRI = *m_NRI;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());

	// Pipeline
	utils::ShaderCodeStorage shaderCodeStorage;
	{
		nri::DescriptorRangeDesc descriptorRangeConstant[2];
		descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER, nri::StageBits::VERTEX_SHADER },
		descriptorRangeConstant[1] = { 0, 1, nri::DescriptorType::STRUCTURED_BUFFER,
			nri::StageBits::VERTEX_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeConstant,
					helper::GetCountOf(descriptorRangeConstant) }
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(CBlock),
			nri::StageBits::VERTEX_SHADER };

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.rootConstantNum = 1;
		pipelineLayoutDesc.rootConstants = &rootConstant;
		pipelineLayoutDesc.descriptorSetNum = helper::GetCountOf(descriptorSetDescs);
		pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_PipelineLayout));

		nri::VertexStreamDesc vertexStreamDesc = {};
		vertexStreamDesc.bindingSlot = 0;
		vertexStreamDesc.stepRate = nri::VertexStreamStepRate::PER_VERTEX;

		// vertexStreamDesc.stepRate = sizeof(Vertex);

		nri::VertexAttributeDesc vertexAttributeDesc[2] = {};
		{
			vertexAttributeDesc[0].format = nri::Format::RGB32_SFLOAT;
			vertexAttributeDesc[0].streamIndex = 0;
			vertexAttributeDesc[0].offset = helper::GetOffsetOf(&VertexA::position);
			vertexAttributeDesc[0].d3d = { "POSITION", 0 };
			vertexAttributeDesc[0].vk.location = { 0 };

			vertexAttributeDesc[1].format = nri::Format::RGBA32_SFLOAT;
			vertexAttributeDesc[1].streamIndex = 0;
			vertexAttributeDesc[1].offset = helper::GetOffsetOf(&VertexA::color);
			vertexAttributeDesc[1].d3d = { "COLOR", 0 };
			vertexAttributeDesc[1].vk.location = { 1 };
		}

		nri::VertexInputDesc vertexInputDesc = {};
		vertexInputDesc.attributes = vertexAttributeDesc;
		vertexInputDesc.attributeNum =
				(uint8_t)helper::GetCountOf(vertexAttributeDesc);
		vertexInputDesc.streams = &vertexStreamDesc;
		vertexInputDesc.streamNum = 1;

		nri::InputAssemblyDesc inputAssemblyDesc = {};
		inputAssemblyDesc.topology = nri::Topology::LINE_LIST;

		nri::RasterizationDesc rasterizationDesc = {};
		rasterizationDesc.fillMode = nri::FillMode::WIREFRAME;
		rasterizationDesc.cullMode = nri::CullMode::NONE;
		// rasterizationDesc.frontCounterClockwise = true;

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
		depthAttachmentDesc.compareFunc = nri::CompareFunc::ALWAYS;
		depthAttachmentDesc.boundsTest = false;

		nri::OutputMergerDesc outputMergerDesc = {};
		outputMergerDesc.colors = &colorAttachmentDesc;
		outputMergerDesc.colorNum = 1;

		nri::ShaderDesc shaderStages[] = {
			utils::LoadShader(deviceDesc.graphicsAPI,
					"debugdraw", shaderCodeStorage, "vs_main"),
			utils::LoadShader(deviceDesc.graphicsAPI, "debugdraw",
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
	}
}

void DebugDrawPass::Render(RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;
	const glm::mat4 p = camera.state.mViewToClip;
	const glm::vec3 cameraPos = camera.state.globalPosition;

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Debug Draw Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		{
			const nri::Viewport viewport = { 0.0f, 0.0f, 900.f,
				600.f, 0.0f, 1.0f };
			NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

			nri::Rect scissor = { 0, 0, 900, 600 };
			NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
		}

		CBlock block = {};
		block.modelMat = glm::mat4(1.0);
		block.modelMat = camera.state.mWorldToView * block.modelMat;
		block.modelMat = p * block.modelMat;

		block.camPos = vec4(cameraPos, 1.0);
		block.index[0] = block.index[1] = block.index[2] = block.index[3] = 0u;
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(CBlock));
		NRI.CmdSetIndexBuffer(info.cmdBuffer, *m_GeometryBuffer->GetBuffer(), 0,
				nri::IndexType::UINT32);
		nri::VertexBufferDesc vertexBufferDesc = {};
		vertexBufferDesc.buffer = m_GeometryBuffer->GetBuffer();
		vertexBufferDesc.offset = m_indicesOffset;
		vertexBufferDesc.stride = sizeof(VertexA);
		NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
		uint32_t instanceCount = (uint32_t)boxWorldMats.size();
		NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(m_indices.size()), instanceCount, 0, 0, 0 });
	}
}

void DebugDrawPass::DrawBox(const glm::vec3 &center, const glm::vec3 &extent, const glm::vec4 &color) {
	glm::mat4 scaleMat = glm::scale(glm::mat4(1.0), extent);
	glm::mat4 translateMat = glm::translate(glm::mat4(1.0), center);
	glm::mat4 worldMat = translateMat * scaleMat;
	boxWorldMats.push_back(worldMat);
}

void DebugDrawPass::GenerateBoxBuffer() {
	auto NRI = *m_NRI;
	{
		m_boxDataBuffer = std::make_shared<Buffer>();
		nri::BufferDesc desc = {};
		desc.size = boxWorldMats.size() * sizeof(BoxMeshData);
		desc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
		desc.structureStride = sizeof(BoxMeshData);

		nri::BufferViewDesc viewDesc = {};
		viewDesc.viewType = nri::BufferViewType::SHADER_RESOURCE;
		viewDesc.size = desc.size;
		viewDesc.format = nri::Format::UNKNOWN;

		m_boxDataBuffer->Create(m_renderer, desc, viewDesc);
	}

	NRI_ABORT_ON_FAILURE(
			NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_PipelineLayout, 0,
					&m_DescriptorSet, 1, 0));
	nri::Descriptor *view = m_boxDataBuffer->GetView();
	nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs = {};
	descriptorRangeUpdateDescs.descriptorNum = 1;
	descriptorRangeUpdateDescs.descriptors = &view;
	NRI.UpdateDescriptorRanges(*m_DescriptorSet, 0, 1, &descriptorRangeUpdateDescs);

	struct BoxMeshData data = {};
	data.worldMat = glm::mat4(1.0);

	nri::BufferUploadDesc desc = {};
	desc.buffer = m_boxDataBuffer->GetBuffer();
	desc.data = boxWorldMats.data();
	desc.dataSize = helper::GetByteSizeOf(boxWorldMats);
	desc.after = { nri::AccessBits::SHADER_RESOURCE,
		nri::StageBits::VERTEX_SHADER };

	NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), nullptr, 0, &desc, 1));
}