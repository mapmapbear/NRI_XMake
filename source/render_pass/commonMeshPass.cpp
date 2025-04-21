#include "commonMeshPass.h"
#include "../renderer.h"
#include "NRIDescs.h"
#include "assimp/scene.h"
#include "glm/ext/matrix_transform.hpp"
#include <stddef.h>
#include <cstddef>
#include <cstdint>
#include <format>
#include <vector>

struct CBlock {
	glm::vec4 camPos;
	uint32_t index;
};

CommonMeshPass::CommonMeshPass(Renderer *renderer, utils::Scene &scene) :
		CommonRenderPass(renderer), m_Scene(scene) {
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

	// Load texture
	std::string path =
			utils::GetFullPath("Textures/UV.dds", utils::DataFolder::ROOT);
	if (!utils::LoadTexture(path, m_texture_albedo_data, true)) {
		printf("Can not found this texture %s", path.c_str());
	}

	path = utils::GetFullPath("DamagedHelmet/glTF/Default_normal.dds", utils::DataFolder::ROOT);
	if (!utils::LoadTexture(path, m_texture_normal_data, true)) {
		printf("Can not found this texture %s", path.c_str());
	}
	uint32_t texSize = m_Scene.materialDatas.size();
	// texSize = 3;
	m_texureDatas.resize(texSize);
	m_textures.resize(texSize);
	uint32_t texCount = 0;
	for (size_t i = 0; i < texSize; ++i) {
		std::string path = m_Scene.materialDatas.at(i).textureMap["BASE"];
		if (!path.empty()) {
			if (!utils::LoadTexture(path, m_texureDatas[i], true)) {
				printf("Can not found this texture %s", path.c_str());
			}
		} else {
			if (!utils::LoadTexture("data/Textures/white.dds", m_texureDatas[i], true)) {
				printf("Can not found this texture %s", path.c_str());
			}
		}
		uint32_t texOffset = 0;
		if (m_texureDatas[i].data.GetFormat() != tinyddsloader::DDSFile::DXGIFormat::Unknown) {
			nri::TextureDesc textureDesc = {};
			textureDesc.type = nri::TextureType::TEXTURE_2D;
			textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
			textureDesc.format = m_texureDatas[i].GetFormat(true);
			textureDesc.width = m_texureDatas[i].GetWidth();
			textureDesc.height = m_texureDatas[i].GetHeight();
			textureDesc.mipNum = 1; //m_texureDatas[i].GetMipNum();
			textureDesc.depth = m_texureDatas[i].GetDepth();
			texOffset = 7 + texCount++;
			NRI_ABORT_ON_FAILURE(
					NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_textures[i]));
		}

		m_texureDatas[i].name = path;
		m_materialIndexBlocks.push_back({ texOffset, 0, 0, 0 });
	}
	//std::erase_if(m_textures, [](nri::Texture *tex) { return tex == nullptr; });
	//std::erase_if(m_texureDatas, [](utils::Texture tex) { return tex.data.GetFormat() == tinyddsloader::DDSFile::DXGIFormat::Unknown; });

	std::cout << std::format("texSize={}, texDataSize={}", m_textures.size(), m_texureDatas.size()) << std::endl;

	// GPU Resource
	const uint32_t constantBufferSize = helper::Align((uint32_t)sizeof(ConstantBufferLayout),
			deviceDesc.memoryAlignment.constantBufferOffset);

	{
		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = constantBufferSize * BUFFERED_FRAME_MAX_NUM;
		bufferDesc.usage = nri::BufferUsageBits::CONSTANT_BUFFER;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateBuffer(*m_renderer->GetRenderDevice(), bufferDesc, m_ConstantBuffer));
	}

	uint64_t previousVertexOffset = 0;
	uint64_t previousIndexAndVertexSize = 0;

	for (size_t i = 0; i < m_Scene.meshDatas.size(); ++i) {
		utils::MeshData &node = m_Scene.meshDatas.at(i);
		std::vector<Vertex> m_positionNode = {};
		for (unsigned int i = 0; i != node.vertices.size(); i++) {
			m_positionNode.push_back({ node.vertices.at(i), node.texCoords.at(i), node.normals.at(i) });
		}
		m_positions.push_back(m_positionNode);
		const uint64_t indexDataSize = helper::GetByteSizeOf(node.indices);
		const uint64_t indexDataAlignedSize = helper::Align(indexDataSize, 32);
		const uint64_t vertexDataSize = helper::GetByteSizeOf(m_positionNode);

		// Compute offsets for the current mesh
		uint64_t indexOffset = (i == 0) ? 0 : previousIndexAndVertexSize;
		uint64_t vertexOffset = (i == 0) ? indexDataAlignedSize : (indexOffset + indexDataAlignedSize);

		// Store the offsets
		m_sceneMeshOffsets.push_back({ indexOffset, vertexOffset });

		// Update running totals
		m_indexDataAlignedTotalSize += indexDataAlignedSize;
		m_vertexDataTotalSize += vertexDataSize;

		// Update for the next mesh
		previousIndexAndVertexSize = (i == 0) ? indexDataAlignedSize + vertexDataSize : (previousIndexAndVertexSize + indexDataAlignedSize + vertexDataSize);
		previousVertexOffset = vertexOffset;
	}

	// Total Geometry
	{
		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = m_indexDataAlignedTotalSize + m_vertexDataTotalSize;
		bufferDesc.usage = nri::BufferUsageBits::VERTEX_BUFFER |
				nri::BufferUsageBits::INDEX_BUFFER;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateBuffer(*m_renderer->GetRenderDevice(), bufferDesc, m_GeometryBuffer));
		NRI.SetDebugName(m_GeometryBuffer, "m_GeometryBuffer");
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = m_texture_albedo_data.GetFormat(true);
		textureDesc.width = m_texture_albedo_data.GetWidth();
		textureDesc.height = m_texture_albedo_data.GetHeight();
		textureDesc.mipNum = 1;
		textureDesc.depth = m_texture_albedo_data.GetDepth();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_texture_albedo));
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = m_texture_normal_data.GetFormat(true);
		textureDesc.width = m_texture_normal_data.GetWidth();
		textureDesc.height = m_texture_normal_data.GetHeight();
		textureDesc.mipNum = 1;
		textureDesc.depth = m_texture_normal_data.GetDepth();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_texture_normal));
	}
}

void CommonMeshPass::BindMemory() {
	auto NRI = *m_NRI;
	// Bind Memory
	std::vector<nri::Buffer *> constantBufferArray = { m_ConstantBuffer };

	nri::ResourceGroupDesc resourceGroupDesc = {};
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::HOST_UPLOAD;
	resourceGroupDesc.bufferNum = constantBufferArray.size();
	resourceGroupDesc.buffers = constantBufferArray.data();

	m_MemoryAllocations.resize(1, nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(*m_renderer->GetRenderDevice(), resourceGroupDesc,
			m_MemoryAllocations.data()));

	std::vector<nri::Buffer *> bufferArray = {
		m_GeometryBuffer
	};

	std::vector<nri::Texture *> textureArray = { m_texture_albedo, m_texture_normal };
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.bufferNum = bufferArray.size();
	resourceGroupDesc.buffers = bufferArray.data();
	resourceGroupDesc.textureNum = textureArray.size();
	resourceGroupDesc.textures = textureArray.data();

	m_MemoryAllocations.resize(
			1 + NRI.CalculateAllocationNumber(*m_renderer->GetRenderDevice(), resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_renderer->GetRenderDevice(), resourceGroupDesc, m_MemoryAllocations.data() + 1));

	resourceGroupDesc = {};
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.textureNum = m_textures.size();
	resourceGroupDesc.textures = m_textures.data();

	m_MemoryAllocations.resize(
			m_MemoryAllocations.size() + NRI.CalculateAllocationNumber(*m_renderer->GetRenderDevice(), resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_renderer->GetRenderDevice(), resourceGroupDesc, m_MemoryAllocations.data() + 1));
	// Descriptors

	m_textureViews.resize(m_textures.size());
	for (size_t i = 0; i < m_textureViews.size(); ++i) {
		nri::Texture2DViewDesc texture2DViewDesc = {
			m_textures[i], nri::Texture2DViewType::SHADER_RESOURCE_2D,
			m_texureDatas[i].GetFormat()
		};
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDesc, m_textureViews[i]));
		SPDLOG_INFO("texOffset= {}\n", m_renderer->texViewOffset++);
	}

	{ // Sampler
		nri::SamplerDesc samplerDesc = {};
		samplerDesc.addressModes = { nri::AddressMode::REPEAT,
			nri::AddressMode::REPEAT, nri::AddressMode::REPEAT };
		samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR,
			nri::Filter::LINEAR };
		samplerDesc.anisotropy = 4;
		samplerDesc.mipMax = 16.0f;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateSampler(*m_renderer->GetRenderDevice(), samplerDesc, m_Sampler));
	}

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

	// Upload data
	{
		std::vector<uint8_t> geometryBufferData(m_indexDataAlignedTotalSize +
				m_vertexDataTotalSize);
		for (size_t i = 0; i < m_Scene.meshDatas.size(); ++i) {
			memcpy(&geometryBufferData[m_sceneMeshOffsets[i].first], m_Scene.meshDatas[i].indices.data(), helper::GetByteSizeOf(m_Scene.meshDatas[i].indices));
			memcpy(&geometryBufferData[m_sceneMeshOffsets[i].second], m_positions.at(i).data(), helper::GetByteSizeOf(m_positions.at(i)));
		}

		nri::BufferUploadDesc bufferData = {};
		bufferData.buffer = m_GeometryBuffer;
		bufferData.data = &geometryBufferData[0];
		bufferData.dataSize = geometryBufferData.size();
		bufferData.after = { nri::AccessBits::INDEX_BUFFER |
			nri::AccessBits::VERTEX_BUFFER };
		std::vector<nri::BufferUploadDesc> uploadDescArray = { bufferData };

		std::vector<nri::TextureUploadDesc> texUploadDescArray(2);
		std::vector<nri::TextureSubresourceUploadDesc> subDataArr(2);

		std::vector<utils::Texture> tex_data_array = { m_texture_albedo_data, m_texture_normal_data }; //, m_texture_normal_data, m_texture_mr_data, m_texture_ao_data, m_texture_emissive_data };
		std::vector<nri::Texture *> tex_array = { m_texture_albedo, m_texture_normal }; //, m_texture_mr, m_texture_ao, m_texture_emissive };

		for (size_t i = 0; i < tex_data_array.size(); i++) {
			auto &tex_data = tex_data_array[i];

			for (uint32_t mip = 0; mip < 1; mip++) {
				auto imgData = tex_data.data.GetImageData(mip, 0);

				subDataArr[i].slices = imgData->m_mem;
				subDataArr[i].sliceNum = 1;
				subDataArr[i].rowPitch = imgData->m_memPitch;
				subDataArr[i].slicePitch = imgData->m_memSlicePitch;
			}

			texUploadDescArray[i].subresources = &subDataArr[i];
			texUploadDescArray[i].texture = tex_array[i];
			texUploadDescArray[i].after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			texUploadDescArray[i].planes = nri::PlaneBits::ALL;
		}

		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), texUploadDescArray.data(), texUploadDescArray.size(),
				uploadDescArray.data(),
				uploadDescArray.size()));
		uint32_t texSize = m_textures.size();
		std::vector<nri::TextureUploadDesc> texUploadDescArray1(texSize);
		std::vector<nri::TextureSubresourceUploadDesc> subDataArr1(texSize);

		for (size_t i = 0; i < m_texureDatas.size(); i++) {
			auto &tex_data = m_texureDatas[i];

			for (uint32_t mip = 0; mip < 1; mip++) {
				auto imgData = tex_data.data.GetImageData(mip, 0);
				subDataArr1[i].slices = imgData->m_mem;
				subDataArr1[i].sliceNum = 1;
				subDataArr1[i].rowPitch = imgData->m_memPitch;
				subDataArr1[i].slicePitch = imgData->m_memSlicePitch;
			}

			texUploadDescArray1[i].subresources = &subDataArr1[i];
			texUploadDescArray1[i].texture = m_textures[i];
			texUploadDescArray1[i].after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			texUploadDescArray1[i].planes = nri::PlaneBits::ALL;
		}
		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), texUploadDescArray1.data(), texUploadDescArray1.size(),
				nullptr,
				0));
	}
}

void CommonMeshPass::BuildPipeline() {
	auto NRI = *m_NRI;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());

	// Pipeline
	utils::ShaderCodeStorage shaderCodeStorage;
	{
		nri::DescriptorRangeDesc descriptorRangeConstant[1];
		descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER,
			nri::StageBits::ALL };

		nri::DescriptorRangeDesc descriptorRangeTexture[2];
		descriptorRangeTexture[0] = { 0, (uint32_t)m_textureViews.size(), nri::DescriptorType::TEXTURE,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[1] = { 0, 1, nri::DescriptorType::SAMPLER,
			nri::StageBits::FRAGMENT_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeConstant,
					helper::GetCountOf(descriptorRangeConstant) },
			{ 1, descriptorRangeTexture, helper::GetCountOf(descriptorRangeTexture) },
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(CBlock),
			nri::StageBits::FRAGMENT_SHADER };

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

		nri::VertexAttributeDesc vertexAttributeDesc[3] = {};
		{
			vertexAttributeDesc[0].format = nri::Format::RGB32_SFLOAT;
			vertexAttributeDesc[0].streamIndex = 0;
			vertexAttributeDesc[0].offset = helper::GetOffsetOf(&Vertex::position);
			vertexAttributeDesc[0].d3d = { "POSITION", 0 };
			vertexAttributeDesc[0].vk.location = { 0 };

			vertexAttributeDesc[1].format = nri::Format::RG32_SFLOAT;
			vertexAttributeDesc[1].streamIndex = 0;
			vertexAttributeDesc[1].offset = helper::GetOffsetOf(&Vertex::uv);
			vertexAttributeDesc[1].d3d = { "TEXCOORD", 0 };
			vertexAttributeDesc[1].vk.location = { 1 };

			vertexAttributeDesc[2].format = nri::Format::RGB32_SFLOAT;
			vertexAttributeDesc[2].streamIndex = 0;
			vertexAttributeDesc[2].offset = helper::GetOffsetOf(&Vertex::normal);
			vertexAttributeDesc[2].d3d = { "NORMAL", 0 };
			vertexAttributeDesc[2].vk.location = { 2 };
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

		nri::ColorAttachmentDesc colorAttachmentDesc = {};
		colorAttachmentDesc.format = nri::Format::RGBA8_UNORM;
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
		outputMergerDesc.depthStencilFormat = nri::Format::D16_UNORM;

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

	// Descriptor sets
	{
		// Texture
		NRI_ABORT_ON_FAILURE(
				NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_PipelineLayout, 1,
						&m_TextureDescriptorSet, 1, 0));

		std::vector<nri::Descriptor *> shaderResoruceViewArray = { m_textureViews[0] }; //, m_texture_normal_view, m_texture_mr_view, m_texture_ao_view, m_texture_emissive_view }; //, m_CubemapTextureShaderResource };

		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = m_textureViews.size();
		descriptorRangeUpdateDescs[0].descriptors = m_textureViews.data();

		descriptorRangeUpdateDescs[1].descriptorNum = 1;
		descriptorRangeUpdateDescs[1].descriptors = &m_Sampler;

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

	ConstantBufferLayout *commonConstants = (ConstantBufferLayout *)NRI.MapBuffer(
			*m_ConstantBuffer, 0,
			sizeof(ConstantBufferLayout));

	const glm::mat4 m1 = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
			glm::vec3(1.0f, 0.f, 0.f));
	const glm::mat4 m2 = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime(),
			glm::vec3(0.0f, 1.f, 0.f));
	const glm::mat4 m3 = glm::scale(glm::mat4(1.0), vec3(15.0));
	const glm::mat4 m4 = glm::translate(glm::mat4(1.0), vec3(0.0, 0.2, 0.0));
	glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.8f, 0.0f)) * m2 * m3 * m4;
	// const glm::mat4 p = glm::perspectiveLH_ZO(glm::radians(m_Fov), 900.f / 600.f, 0.1f, 100.0f);
	const glm::mat4 p = camera.state.mViewToClip;
	const glm::vec3 cameraPos = camera.state.globalPosition;
	glm::vec3 target = cameraPos + glm::vec3(camera.state.mWorldToView[0][2], camera.state.mWorldToView[1][2], camera.state.mWorldToView[2][2]);
	const glm::mat4 v = glm::lookAtLH(cameraPos, target, glm::vec3(0.0f, 1.0f, 0.0f));

	if (commonConstants) {
		commonConstants->modelMat = m;
		commonConstants->viewMat = camera.state.mWorldToView;
		commonConstants->projectMat = p;
		NRI.UnmapBuffer(*m_ConstantBuffer);
	}

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "SimpleModelMesh");

		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		for (size_t i = 0; i < m_Scene.meshDatas.size(); ++i) {
			CBlock block = {};
			block.camPos = vec4(cameraPos, 1.0);
			uint32_t index = m_materialIndexBlocks.at(m_Scene.meshDatas.at(i).materialIndex).textureIndex;
			block.index = index; //m_renderer->testIndex;
			NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(CBlock));

			uint32_t indexOffset = m_sceneMeshOffsets.at(i).first;
			uint32_t vertexOffset = m_sceneMeshOffsets.at(i).second;
			uint32_t indexCount = m_Scene.meshDatas.at(i).indices.size();
			NRI.CmdSetIndexBuffer(info.cmdBuffer, *m_GeometryBuffer, indexOffset,
					nri::IndexType::UINT32);
			nri::VertexBufferDesc vertexBufferDesc = {};
			vertexBufferDesc.buffer = m_GeometryBuffer;
			vertexBufferDesc.offset = vertexOffset;
			vertexBufferDesc.stride = sizeof(Vertex);
			NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
			NRI.CmdSetDescriptorSet(info.cmdBuffer, 0,
					*m_ConstantBufferDescriptorSet, nullptr);
			NRI.CmdSetDescriptorSet(info.cmdBuffer, 1, *m_TextureDescriptorSet,
					nullptr);
			{
				const nri::Viewport viewport = { 0.0f, 0.0f, 900.f,
					600.f, 0.0f, 1.0f };
				NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

				nri::Rect scissor = { 0, 0, 900, 600 };
				NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
			}
			uint32_t instanceCount = 1;
			NRI.CmdDrawIndexed(info.cmdBuffer, { indexCount, instanceCount, 0, 0, 0 });
		}
	}
}
