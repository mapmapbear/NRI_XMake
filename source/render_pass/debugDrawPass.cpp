#include "debugDrawPass.h"
#include "NRIDescs.h"
#include "buffer.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/matrix.hpp"
#include "glm/trigonometric.hpp"
#include "renderer.h"
#include <cstdint>
#include <memory>

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

void DebugDrawPass::GenerateBoundingSphere(float radius, int segments) {
	// 清空之前的数据
	m_positions_sphere.clear();
	m_positions_sphere.clear();

	// 生成三个主轴圆环
	const float PI = 3.14159265358979323846f;
	const float step = 2.0f * PI / segments;

	// 颜色：X轴圆环为红色，Y轴圆环为绿色，Z轴圆环为蓝色
	glm::vec4 xAxisColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // 红色
	glm::vec4 yAxisColor = { 0.0f, 1.0f, 0.0f, 1.0f }; // 绿色
	glm::vec4 zAxisColor = { 0.0f, 0.0f, 1.0f, 1.0f }; // 蓝色

	// 生成XY平面圆环（Z轴）
	uint32_t baseIndex = 0;
	for (int i = 0; i < segments; i++) {
		float angle = i * step;
		float nextAngle = (i + 1) * step;

		// 当前点
		float x = radius * cos(angle);
		float y = radius * sin(angle);
		m_positions_sphere.push_back({ { x, y, 0.0f }, { x, y, 0.0f, 1.0f } });

		// 添加线段索引
		m_indices_sphere.push_back(baseIndex + i);
		m_indices_sphere.push_back(baseIndex + ((i + 1) % segments));
	}

	// // 生成XZ平面圆环（Y轴）
	baseIndex = segments;
	for (int i = 0; i < segments; i++) {
		float angle = i * step;
		float nextAngle = (i + 1) * step;

		// 当前点
		float x = radius * cos(angle);
		float z = radius * sin(angle);
		m_positions_sphere.push_back({ { x, 0.0f, z }, { x, 0.0f, z, 1.0f } });

		// 添加线段索引
		m_indices_sphere.push_back(baseIndex + i);
		m_indices_sphere.push_back(baseIndex + ((i + 1) % segments));
	}
	for (int i = 0; i < 8; ++i) {
		m_indices_sphere.push_back(baseIndex + i);
		m_indices_sphere.push_back(baseIndex + ((i + 1) % segments));
	}

	float stepOffset = 24 * step;
	// // 生成YZ平面圆环（X轴）
	baseIndex = segments * 2;
	for (int i = 0; i < segments; i++) {
		float angle = i * step + stepOffset;

		// 当前点
		float y = radius * cos(angle);
		float z = radius * sin(angle);
		m_positions_sphere.push_back({ { 0.0f, y, z }, { 0.0f, y, z, 1.0f } });

		// 添加线段索引
		m_indices_sphere.push_back(baseIndex + i);
		m_indices_sphere.push_back(baseIndex + ((i + 1) % segments));
	}
}

void DebugDrawPass::GenerateFrustum(float nearZ, float farZ, float fov, float aspect) {
	float halfFovY = fov / 2.0f;
	float tanHalfFovY = tanf(halfFovY);

	float nearPlaneHeight = 2.0f * tanHalfFovY * nearZ;
	float nearPlaneWidth = nearPlaneHeight * aspect;

	float farPlaneHeight = 2.0f * tanHalfFovY * farZ;
	float farPlaneWidth = farPlaneHeight * aspect;

	m_positions_frustum.clear();
	m_indices_frustum.clear();

	glm::vec4 color = { 1.0f, 1.0f, 0.0f, 1.0f };

	m_positions_frustum.push_back({ { -nearPlaneWidth / 2.0f, -nearPlaneHeight / 2.0f, -nearZ }, color }); //near Left Top
	m_positions_frustum.push_back({ { nearPlaneWidth / 2.0f, -nearPlaneHeight / 2.0f, -nearZ }, color }); //near Right Top
	m_positions_frustum.push_back({ { nearPlaneWidth / 2.0f, nearPlaneHeight / 2.0f, -nearZ }, color }); //near Right Bottom
	m_positions_frustum.push_back({ { -nearPlaneWidth / 2.0f, nearPlaneHeight / 2.0f, -nearZ }, color }); //near Left Bottom

	m_positions_frustum.push_back({ { -farPlaneWidth / 2.0f, -farPlaneHeight / 2.0f, -farZ }, color }); //far Left Top
	m_positions_frustum.push_back({ { farPlaneWidth / 2.0f, -farPlaneHeight / 2.0f, -farZ }, color }); //far Right Top
	m_positions_frustum.push_back({ { farPlaneWidth / 2.0f, farPlaneHeight / 2.0f, -farZ }, color }); //far Right Bottom
	m_positions_frustum.push_back({ { -farPlaneWidth / 2.0f, farPlaneHeight / 2.0f, -farZ }, color }); //far Left Bottom

	// 在近平面和远平面之间添加3个中间平面
	int max_Frustum_Level = 7;
	for (int i = 0; i < max_Frustum_Level; i++) {
		float dist = nearZ + (farZ - nearZ) * ((i + 1.f) / (max_Frustum_Level + 1));
		float planeHeight = 2.0f * tanHalfFovY * dist;
		float planeWidth = planeHeight * aspect;

		// 添加中间平面的4个顶点
		m_positions_frustum.push_back({ { -planeWidth / 2.0f, -planeHeight / 2.0f, -dist }, color });
		m_positions_frustum.push_back({ { planeWidth / 2.0f, -planeHeight / 2.0f, -dist }, color });
		m_positions_frustum.push_back({ { planeWidth / 2.0f, planeHeight / 2.0f, -dist }, color });
		m_positions_frustum.push_back({ { -planeWidth / 2.0f, planeHeight / 2.0f, -dist }, color });
	}

	m_indices_frustum = {
		0,
		1,
		1,
		2,
		2,
		3,
		3,
		0,
		4,
		5,
		5,
		6,
		6,
		7,
		7,
		4,
		0,
		4,
		1,
		5,
		2,
		6,
		3,
		7,
		8,
		9,
		9,
		10,
		10,
		11,
		11,
		8,
		12,
		13,
		13,
		14,
		14,
		15,
		15,
		12,
		16,
		17,
		17,
		18,
		18,
		19,
		19,
		16,
		20,
		21,
		21,
		22,
		22,
		23,
		23,
		20,
		24,
		25,
		25,
		26,
		26,
		27,
		27,
		24,
		28,
		29,
		29,
		30,
		30,
		31,
		31,
		28,
		32,
		33,
		33,
		34,
		34,
		35,
		35,
		32,
	};
}

void DebugDrawPass::BindMemory() {
	auto NRI = *m_NRI;
	// Box Mesh
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
		{ { minX, maxY, maxZ }, { minX, maxY, maxZ, 1.0 } } // color }   // 7
	};

	m_indices = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

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

	// Sphere Mesh
	GenerateBoundingSphere(1.0f, 32);

	std::vector<uint8_t> geometryBufferData(indexDataAlignedSize +
			vertexDataSize);
	memcpy(&geometryBufferData[0], m_indices.data(), indexDataSize);
	memcpy(&geometryBufferData[indexDataAlignedSize], m_positions.data(),
			vertexDataSize);

	const uint64_t indexDataSize_sphere = helper::GetByteSizeOf(m_indices_sphere);
	const uint64_t indexDataAlignedSize_sphere = helper::Align(indexDataSize_sphere, 32);
	const uint64_t vertexDataSize_sphere = helper::GetByteSizeOf(m_positions_sphere);
	m_indicesOffset_sphere = indexDataAlignedSize_sphere;

	{
		m_GeometryBuffer_sphere = std::make_shared<Buffer>();
		nri::BufferDesc desc = {};
		desc.size = indexDataAlignedSize_sphere + vertexDataSize_sphere;
		desc.usage = nri::BufferUsageBits::VERTEX_BUFFER | nri::BufferUsageBits::INDEX_BUFFER;

		nri::BufferViewDesc viewDesc = {};
		m_GeometryBuffer_sphere->Create(m_renderer, desc, viewDesc);
	}

	std::vector<uint8_t> geometryBufferData_sphere(indexDataAlignedSize_sphere +
			vertexDataSize_sphere);
	memcpy(&geometryBufferData_sphere[0], m_indices_sphere.data(), indexDataSize_sphere);
	memcpy(&geometryBufferData_sphere[indexDataAlignedSize_sphere], m_positions_sphere.data(),
			vertexDataSize_sphere);

	// Frustum Mesh
	GenerateFrustum(1.0f, 200.0f, glm::radians(90.0f), 16.0f / 9.0f);

	const uint64_t indexDataSize_frustum = helper::GetByteSizeOf(m_indices_frustum);
	const uint64_t indexDataAlignedSize_frustum = helper::Align(indexDataSize_frustum, 32);
	const uint64_t vertexDataSize_frustum = helper::GetByteSizeOf(m_positions_frustum);
	m_indicesOffset_frustum = indexDataAlignedSize_frustum;

	{
		m_GeometryBuffer_frustum = std::make_shared<Buffer>();
		nri::BufferDesc desc = {};
		desc.size = indexDataAlignedSize_frustum + vertexDataSize_frustum;
		desc.usage = nri::BufferUsageBits::VERTEX_BUFFER | nri::BufferUsageBits::INDEX_BUFFER;

		nri::BufferViewDesc viewDesc = {};
		m_GeometryBuffer_frustum->Create(m_renderer, desc, viewDesc);
	}

	std::vector<uint8_t> geometryBufferData_frustum(indexDataAlignedSize_frustum +
			vertexDataSize_frustum);
	memcpy(&geometryBufferData_frustum[0], m_indices_frustum.data(), indexDataSize_frustum);
	memcpy(&geometryBufferData_frustum[indexDataAlignedSize_frustum], m_positions_frustum.data(),
			vertexDataSize_frustum);

	nri::BufferUploadDesc bufferData = {};
	bufferData.buffer = m_GeometryBuffer->GetBuffer();
	bufferData.data = &geometryBufferData[0];
	// bufferData.dataSize = geometryBufferData.size();
	bufferData.after = { nri::AccessBits::INDEX_BUFFER |
		nri::AccessBits::VERTEX_BUFFER };

	nri::BufferUploadDesc bufferData_sphere = {};
	bufferData_sphere.buffer = m_GeometryBuffer_sphere->GetBuffer();
	bufferData_sphere.data = &geometryBufferData_sphere[0];
	// bufferData_sphere.dataSize = geometryBufferData_sphere.size();
	bufferData_sphere.after = { nri::AccessBits::INDEX_BUFFER |
		nri::AccessBits::VERTEX_BUFFER };

	nri::BufferUploadDesc bufferData_frustum = {};
	bufferData_frustum.buffer = m_GeometryBuffer_frustum->GetBuffer();
	bufferData_frustum.data = &geometryBufferData_frustum[0];
	// bufferData_frustum.dataSize = geometryBufferData_frustum.size();
	bufferData_frustum.after = { nri::AccessBits::INDEX_BUFFER |
		nri::AccessBits::VERTEX_BUFFER };

	std::vector<nri::BufferUploadDesc> bufferDataArray = { bufferData, bufferData_sphere, bufferData_frustum };
	NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), nullptr, (uint32_t)0,
			bufferDataArray.data(), (uint32_t)bufferDataArray.size()));
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

		nri::RootConstantDesc rootConstant = { 1, sizeof(ConstantBlock),
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

		nri::InputAssemblyDesc inputAssemblyDesc_sphere = {};
		inputAssemblyDesc_sphere.topology = nri::Topology::LINE_STRIP;

		nri::RasterizationDesc rasterizationDesc = {};
		rasterizationDesc.fillMode = nri::FillMode::SOLID;
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

		graphicsPipelineDesc.inputAssembly = inputAssemblyDesc_sphere;
		NRI_ABORT_ON_FAILURE(NRI.CreateGraphicsPipeline(
				*m_renderer->GetRenderDevice(), graphicsPipelineDesc, m_Pipeline_sphere));
	}
}

void DebugDrawPass::Render(RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;
	const glm::mat4 p = camera.state.mViewToClip;
	const glm::vec3 cameraPos = camera.state.globalPosition;

	ConstantBufferLayout *commonConstants = (ConstantBufferLayout *)NRI.MapBuffer(
			*m_ConstantBuffer, 0,
			sizeof(ConstantBufferLayout));

	if (commonConstants) {
		commonConstants->modelMat = glm::mat4(1.0);
		commonConstants->viewMat = camera.state.mWorldToView;
		commonConstants->projectMat = p;
		NRI.UnmapBuffer(*m_ConstantBuffer);
	}

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "Debug Draw Pass");

		{
			const nri::Viewport viewport = { 0.0f, 0.0f, (float)m_renderer->m_OutputResolution.first,
				(float)m_renderer->m_OutputResolution.second, 0.0f, 1.0f };
			NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

			nri::Rect scissor = { 0, 0, (uint16_t)m_renderer->m_OutputResolution.first, (uint16_t)m_renderer->m_OutputResolution.second };
			NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
		}

		// Box Draw
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		NRI.CmdSetDescriptorSet(info.cmdBuffer, 0, *m_DescriptorSet, nullptr);
		ConstantBlock block = {};
		block.modelMat = glm::mat4(1.0);
		block.modelMat = camera.state.mWorldToView * block.modelMat;
		block.modelMat = p * block.modelMat;
		block.camPos = vec4(cameraPos, 1.0);
		block.index[0] = block.index[1] = block.index[2] = block.index[3] = 0u;
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(ConstantBlock));
		NRI.CmdSetIndexBuffer(info.cmdBuffer, *m_GeometryBuffer->GetBuffer(), 0,
				nri::IndexType::UINT32);
		nri::VertexBufferDesc vertexBufferDesc = {};
		vertexBufferDesc.buffer = m_GeometryBuffer->GetBuffer();
		vertexBufferDesc.offset = m_indicesOffset;
		vertexBufferDesc.stride = sizeof(VertexA);
		NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
		if (m_renderer->m_config.DebugBoxDraw) {
			NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(m_indices.size()), m_box_count, 0, 0, 0 });
		}
		// Sphere Draw
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline_sphere);
		NRI.CmdSetDescriptorSet(info.cmdBuffer, 0, *m_DescriptorSet, nullptr);
		block.index[0] = m_box_count;
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(ConstantBlock));
		NRI.CmdSetIndexBuffer(info.cmdBuffer, *m_GeometryBuffer_sphere->GetBuffer(), 0,
				nri::IndexType::UINT32);
		nri::VertexBufferDesc vertexBufferDesc_sphere = {};
		vertexBufferDesc_sphere.buffer = m_GeometryBuffer_sphere->GetBuffer();
		vertexBufferDesc_sphere.offset = m_indicesOffset_sphere;
		vertexBufferDesc_sphere.stride = sizeof(VertexA);
		NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc_sphere, 1);
		if (m_renderer->m_config.DebugSphereDraw) {
			NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(m_indices_sphere.size()), m_sphere_count, 0, 0, 0 });
		}

		// Frustum Draw
		block.modelMat = glm::inverse(camera.statePrev.mWorldToView) * glm::rotate(glm::mat4(1.0), glm::radians(180.f), glm::vec3(0.0f, 1.0f, 0.0f));

		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		NRI.CmdSetDescriptorSet(info.cmdBuffer, 0, *m_DescriptorSet, nullptr);
		block.index[0] = m_sphere_count + m_box_count;
		block.index[1] = 3;
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(ConstantBlock));
		NRI.CmdSetIndexBuffer(info.cmdBuffer, *m_GeometryBuffer_frustum->GetBuffer(), 0,
				nri::IndexType::UINT32);
		nri::VertexBufferDesc vertexBufferDesc_frustum = {};
		vertexBufferDesc_frustum.buffer = m_GeometryBuffer_frustum->GetBuffer();
		vertexBufferDesc_frustum.offset = m_indicesOffset_frustum;
		vertexBufferDesc_frustum.stride = sizeof(VertexA);
		NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc_frustum, 1);

		NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(m_indices_frustum.size()), 1, 0, 0, 0 });
	}
}

void DebugDrawPass::DrawBox(const glm::vec3 &center, const glm::vec3 &extent, const glm::vec4 &color) {
	glm::mat4 scaleMat = glm::scale(glm::mat4(1.0), extent);
	glm::mat4 translateMat = glm::translate(glm::mat4(1.0), center);
	glm::mat4 worldMat = translateMat * scaleMat;
	boxWorldMats.push_back(worldMat);
	m_box_count++;
}

void DebugDrawPass::DrawFrustum(const glm::vec3 &center) {
	glm::mat4 scaleMat = glm::mat4(1.0);
	glm::mat4 translateMat = glm::translate(glm::mat4(1.0), center);
	glm::mat4 worldMat = translateMat * scaleMat;
	boxWorldMats.push_back(worldMat);
	m_frustum_count++;
}

void DebugDrawPass::DrawSphere(const glm::vec3 &center, float radius, const glm::vec4 &color) {
	glm::mat4 scaleMat = glm::scale(glm::mat4(1.0), glm::vec3(radius));
	glm::mat4 translateMat = glm::translate(glm::mat4(1.0), center);
	glm::mat4 worldMat = translateMat * scaleMat;
	boxWorldMats.push_back(worldMat);
	m_sphere_count++;
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
	nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
	descriptorRangeUpdateDescs[0].descriptorNum = 1;
	descriptorRangeUpdateDescs[0].descriptors = &m_ConstantBufferView;
	descriptorRangeUpdateDescs[1].descriptorNum = 1;
	descriptorRangeUpdateDescs[1].descriptors = &view;

	NRI.UpdateDescriptorRanges(*m_DescriptorSet, 0, 2, descriptorRangeUpdateDescs);

	struct BoxMeshData data = {};
	data.worldMat = glm::mat4(1.0);

	nri::BufferUploadDesc desc = {};
	desc.buffer = m_boxDataBuffer->GetBuffer();
	desc.data = boxWorldMats.data();
	// desc.dataSize = helper::GetByteSizeOf(boxWorldMats);
	desc.after = { nri::AccessBits::SHADER_RESOURCE,
		nri::StageBits::VERTEX_SHADER };

	NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), nullptr, 0, &desc, 1));
}