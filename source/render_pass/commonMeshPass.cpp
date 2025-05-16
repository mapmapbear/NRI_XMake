#include "commonMeshPass.h"
#include "../renderer.h"
#include "NRIDescs.h"
#include "assimp/scene.h"
#include "buffer.h"
#include "glm/ext/matrix_transform.hpp"
#include "mesh.h"
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

	uint32_t texSize = m_Scene.materialDatas.size();
	uint32_t matTexCount = 3;
	m_texureDatas.resize(texSize * matTexCount);
	m_textures.resize(texSize * matTexCount);
	uint32_t texCount = 0;
	size_t texIndex = 0;

	uint32_t texOffset = 0;
	uint32_t texOffset1 = 0;
	uint32_t texOffset2 = 0;

	for (size_t i = 0; i < texSize; i++) {
		{
			std::string path = {};
			if (m_Scene.materialDatas.at(i).textureMap.find("BASE") != m_Scene.materialDatas.at(i).textureMap.end()) {
				path = m_Scene.materialDatas.at(i).textureMap["BASE"];
			}
			if (!path.empty()) {
				if (!utils::LoadTexture(path, m_texureDatas[texIndex], true)) {
					printf("Can not found this texture %s", path.c_str());
				}
			} else {
				m_texureDatas[texIndex] = m_renderer->GetDefaultWhiteTex();
			}
			if (m_texureDatas[texIndex].GetFormat() != nri::Format::UNKNOWN) {
				nri::TextureDesc textureDesc = {};
				textureDesc.type = nri::TextureType::TEXTURE_2D;
				textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
				textureDesc.format = m_texureDatas[texIndex].GetFormat();
				textureDesc.width = m_texureDatas[texIndex].GetWidth();
				textureDesc.height = m_texureDatas[texIndex].GetHeight();
				textureDesc.mipNum = 1; //m_texureDatas[i].GetMipNum();
				textureDesc.depth = m_texureDatas[texIndex].GetDepth();
				texOffset = 7 + texCount++;
				NRI_ABORT_ON_FAILURE(
						NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_textures[texIndex]));
				NRI.SetDebugName(m_textures[texIndex], path.c_str());
			}
			m_texureDatas[texIndex].name = path;
		}
		texIndex++;

		{
			std::string path = {};
			if (m_Scene.materialDatas.at(i).textureMap.find("NORMAL") != m_Scene.materialDatas.at(i).textureMap.end()) {
				path = m_Scene.materialDatas.at(i).textureMap["NORMAL"];
			}
			if (!path.empty()) {
				if (!utils::LoadTexture(path, m_texureDatas[texIndex], true)) {
					printf("Can not found this texture %s", path.c_str());
				}
			} else {
				m_texureDatas[texIndex] = m_renderer->GetDefaultNormalTex();
			}

			if (m_texureDatas[texIndex].GetFormat() != nri::Format::UNKNOWN) {
				nri::TextureDesc textureDesc = {};
				textureDesc.type = nri::TextureType::TEXTURE_2D;
				textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
				textureDesc.format = m_texureDatas[texIndex].GetFormat();
				textureDesc.width = m_texureDatas[texIndex].GetWidth();
				textureDesc.height = m_texureDatas[texIndex].GetHeight();
				textureDesc.mipNum = 1; //m_texureDatas[i].GetMipNum();
				textureDesc.depth = m_texureDatas[texIndex].GetDepth();
				texOffset1 = 7 + texCount++;
				NRI_ABORT_ON_FAILURE(
						NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_textures[texIndex]));
				// NRI.GetTextureNativeObject(*m_textures[texIndex]).SetName(path.c_str());
				NRI.SetDebugName(m_textures[texIndex], path.c_str());
			}

			m_texureDatas[texIndex].name = path;
		}
		texIndex++;

		{
			std::string path = {};
			if (m_Scene.materialDatas.at(i).textureMap.find("METALLIC") != m_Scene.materialDatas.at(i).textureMap.end()) {
				path = m_Scene.materialDatas.at(i).textureMap["METALLIC"];
			}
			if (!path.empty()) {
				if (!utils::LoadTexture(path, m_texureDatas[texIndex], true)) {
					printf("Can not found this texture %s", path.c_str());
				}
			} else {
				m_texureDatas[texIndex] = m_renderer->GetDefaultWhiteTex();
			}
			if (m_texureDatas[texIndex].GetFormat() != nri::Format::UNKNOWN) {
				nri::TextureDesc textureDesc = {};
				textureDesc.type = nri::TextureType::TEXTURE_2D;
				textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
				textureDesc.format = m_texureDatas[texIndex].GetFormat();
				textureDesc.width = m_texureDatas[texIndex].GetWidth();
				textureDesc.height = m_texureDatas[texIndex].GetHeight();
				textureDesc.mipNum = 1; //m_texureDatas[i].GetMipNum();
				textureDesc.depth = m_texureDatas[texIndex].GetDepth();
				texOffset2 = 7 + texCount++;
				NRI_ABORT_ON_FAILURE(
						NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_textures[texIndex]));
				NRI.SetDebugName(m_textures[texIndex], path.c_str());
			}
			m_texureDatas[texIndex].name = path;
		}
		texIndex++;

		m_materialIndexBlocks.push_back({ texOffset, texOffset1, texOffset2, 0 });
	}
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

	m_Scene.meshes.resize(m_Scene.meshDatas.size());

	for (size_t i = 0; i < m_Scene.meshDatas.size(); ++i) {
		utils::MeshData &node = m_Scene.meshDatas.at(i);
		utils::Mesh &staticMesh = m_Scene.meshes.at(i);
		m_positions.push_back(node.m_vertexesData);
		const uint64_t indexDataAlignedSize = helper::Align(helper::GetByteSizeOf(node.indices), 32);
		const uint64_t vertexDataSize = helper::GetByteSizeOf(node.m_vertexesData);

		// Compute offsets for the current mesh
		uint64_t indexOffset = (i == 0) ? 0 : previousIndexAndVertexSize;
		uint64_t vertexOffset = (i == 0) ? indexDataAlignedSize : (indexOffset + indexDataAlignedSize);

		// Store the offsets
		m_sceneMeshOffsets.push_back({ indexOffset, vertexOffset });
		staticMesh.indexOffset = indexOffset;
		staticMesh.vertexOffset = vertexOffset;
		staticMesh.indexNum = node.indices.size();
		staticMesh.vertexNum = node.vertices.size();
		// Update running totals
		m_indexDataAlignedTotalSize += indexDataAlignedSize;
		m_vertexDataTotalSize += vertexDataSize;

		// Update for the next mesh
		previousIndexAndVertexSize = (i == 0) ? indexDataAlignedSize + vertexDataSize : (previousIndexAndVertexSize + indexDataAlignedSize + vertexDataSize);
		previousVertexOffset = vertexOffset;
	}

	GetMeshNode(m_Scene.rootNode);

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
#if 1
	std::vector<nri::Buffer *> bufferArray = {
		m_GeometryBuffer
	};

	resourceGroupDesc = {};
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.bufferNum = (uint32_t)bufferArray.size();
	resourceGroupDesc.buffers = bufferArray.data();
	resourceGroupDesc.textureNum = (uint32_t)m_textures.size();
	resourceGroupDesc.textures = m_textures.data();

	m_MemoryAllocations.resize(
			m_MemoryAllocations.size() + NRI.CalculateAllocationNumber(*m_renderer->GetRenderDevice(), resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_renderer->GetRenderDevice(), resourceGroupDesc, m_MemoryAllocations.data() + 1));
	// Descriptors
	for (size_t i = 0; i < m_rootMesh->GetMeshCount(); ++i) {
		auto subMesh = m_rootMesh->GetMesh(i);
		const Material mat = m_rootMesh->GetMaterial(subMesh->GetMaterialID());
		m_matTexSet.insert(mat.m_BaseTexture);
		m_matTexSet.insert(mat.m_NormalTexture);
		m_matTexSet.insert(mat.m_MetallicTexture);
	}

	m_textureViews.resize(m_textures.size() + 3);
	for (size_t i = 0; i < m_textures.size(); ++i) {
		nri::Texture2DViewDesc texture2DViewDesc = {
			m_textures[i], nri::Texture2DViewType::SHADER_RESOURCE_2D,
			m_texureDatas[i].GetFormat()
		};
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDesc, m_textureViews[i]));
		SPDLOG_INFO("texOffset= {}\n", m_renderer->texViewOffset++);
	}
	m_brdfTexIndex = m_renderer->texViewOffset;
	nri::Texture2DViewDesc texture2DViewDesc1 = { .texture = m_renderer->m_DiffuseIrradianceTex, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_CUBE, .format = m_renderer->diffuseIrradianceTex.GetFormat(), .mipOffset = 0, .mipNum = m_renderer->diffuseIrradianceTex.GetMipNum(), .layerOffset = 0, .layerNum = 6 };

	NRI_ABORT_ON_FAILURE(
			NRI.CreateTexture2DView(texture2DViewDesc1, m_textureViews[m_textures.size()]));

	nri::Texture2DViewDesc texture2DViewDesc2 = { .texture = m_renderer->m_SpecularIrradianceTex, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_CUBE, .format = m_renderer->specularIrradianceTex.GetFormat(), .mipOffset = 0, .mipNum = m_renderer->specularIrradianceTex.GetMipNum(), .layerOffset = 0, .layerNum = 6 };

	NRI_ABORT_ON_FAILURE(
			NRI.CreateTexture2DView(texture2DViewDesc2, m_textureViews[m_textures.size() + 1]));

	nri::Texture2DViewDesc texture2DViewDes3 = { .texture = m_renderer->m_BRDFTex, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = m_renderer->BRDFTex.GetFormat() };
	NRI_ABORT_ON_FAILURE(
			NRI.CreateTexture2DView(texture2DViewDes3, m_textureViews[m_textures.size() + 2]));

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
		for (size_t i = 0; i < m_Scene.meshes.size(); ++i) {
			auto &meshNode = m_Scene.meshes[i];
			memcpy(&geometryBufferData[meshNode.indexOffset], m_Scene.meshDatas[i].indices.data(), helper::GetByteSizeOf(m_Scene.meshDatas[i].indices));
			memcpy(&geometryBufferData[meshNode.vertexOffset], m_positions.at(i).data(), helper::GetByteSizeOf(m_positions.at(i)));
		}

		nri::BufferUploadDesc bufferData = {};
		bufferData.buffer = m_GeometryBuffer;
		bufferData.data = &geometryBufferData[0];
		bufferData.dataSize = geometryBufferData.size();
		bufferData.after = { nri::AccessBits::INDEX_BUFFER |
			nri::AccessBits::VERTEX_BUFFER };
		std::vector<nri::BufferUploadDesc> uploadDescArray = { bufferData };

		uint32_t texSize = (uint32_t)m_textures.size();
		std::vector<nri::TextureUploadDesc> texUploadDescArray(texSize);
		std::vector<nri::TextureSubresourceUploadDesc> subDataArr(texSize);

		for (size_t i = 0; i < m_texureDatas.size(); i++) {
			auto &tex_data = m_texureDatas[i];
			if (tex_data.isDDS) {
				for (uint32_t mip = 0; mip < 1; mip++) {
					auto imgData = tex_data.data.GetImageData(mip, 0);
					subDataArr[i].slices = imgData->m_mem;
					subDataArr[i].sliceNum = 1;
					subDataArr[i].rowPitch = imgData->m_memPitch;
					subDataArr[i].slicePitch = imgData->m_memSlicePitch;
				}
			} else {
				for (uint32_t mip = 0; mip < 1; mip++) {
					tex_data.GetSubresource(subDataArr[i], mip);
				}
			}

			texUploadDescArray[i].subresources = &subDataArr[i];
			texUploadDescArray[i].texture = m_textures[i];
			texUploadDescArray[i].after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			texUploadDescArray[i].planes = nri::PlaneBits::ALL;
		}
		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), texUploadDescArray.data(), (uint32_t)texUploadDescArray.size(),
				uploadDescArray.data(),
				(uint32_t)uploadDescArray.size()));
	}
#endif

	//TODO
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
		// descriptorRangeTexture[0] = { 0, (uint32_t)m_textureViews.size(), nri::DescriptorType::TEXTURE,
		descriptorRangeTexture[0] = { 0, 999, nri::DescriptorType::TEXTURE,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[1] = { 0, 1, nri::DescriptorType::SAMPLER,
			nri::StageBits::FRAGMENT_SHADER };

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
		rasterizationDesc.cullMode = nri::CullMode::BACK;
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

	// add temp descriptors
	uint32_t newMatTexIndex = m_brdfTexIndex + 3;
	{
		for (auto &tex : m_matTexSet) {
			nri::Descriptor *view = tex->GetView();
			tex->SetViewIndex(newMatTexIndex);
			m_textureViews.push_back(view);
		}
	}

	// Descriptor sets
	{
		// Texture
		NRI_ABORT_ON_FAILURE(
				NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_PipelineLayout, 1,
						&m_TextureDescriptorSet, 1, 0));

		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = (uint32_t)m_textureViews.size();
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

	const glm::mat4 p = camera.state.mViewToClip;
	const glm::vec3 cameraPos = camera.state.globalPosition;
	if (commonConstants) {
		commonConstants->modelMat = glm::mat4(1.0);
		commonConstants->viewMat = camera.state.mWorldToView;
		commonConstants->projectMat = p;
		NRI.UnmapBuffer(*m_ConstantBuffer);
	}

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "SimpleModelMesh");

		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		glm::mat4 m1 = glm::mat4(1.0);
		for (uint32_t meshIndex = 0; meshIndex < m_meshNodes.size(); ++meshIndex) {
			m1 = m_meshNodes.at(meshIndex).globalTransform;

			for (uint16_t i = 0; i < m_meshNodes[meshIndex].meshIndices.size(); ++i) {
				CBlock block = {};
				block.modelMat = m1;
				block.camPos = vec4(cameraPos, 1.0);
				uint32_t index = m_materialIndexBlocks.at(m_Scene.meshDatas.at(i).materialIndex).textureIndex;
				uint32_t index1 = m_materialIndexBlocks.at(m_Scene.meshDatas.at(i).materialIndex).textureIndex1;
				uint32_t index2 = m_materialIndexBlocks.at(m_Scene.meshDatas.at(i).materialIndex).textureIndex2;
				block.index[0] = index; //m_renderer->testIndex;
				block.index[1] = index1;
				block.index[2] = index2;
				block.index[3] = m_brdfTexIndex;
				utils::MaterialData matData = m_Scene.materialDatas.at(m_Scene.meshDatas.at(i).materialIndex);
				block.baseColor = matData.baseColor;
				block.pbrParams = { matData.metallic, matData.roughness, 0.0, 0.0 };
				block.testVec = { m_renderer->testRoughness, m_renderer->testMaterial, 0.0, 0.0 };

				NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(CBlock));

				uint32_t indexOffset = (uint32_t)m_Scene.meshes.at(m_meshNodes[meshIndex].meshIndices[i]).indexOffset;
				uint32_t vertexOffset = (uint32_t)m_Scene.meshes.at(m_meshNodes[meshIndex].meshIndices[i]).vertexOffset;
				uint32_t indexCount = (uint32_t)m_Scene.meshes.at(m_meshNodes[meshIndex].meshIndices[i]).indexNum;
				NRI.CmdSetIndexBuffer(info.cmdBuffer, *m_GeometryBuffer, indexOffset,
						nri::IndexType::UINT32);
				nri::VertexBufferDesc vertexBufferDesc = {};
				vertexBufferDesc.buffer = m_GeometryBuffer;
				vertexBufferDesc.offset = vertexOffset;
				vertexBufferDesc.stride = sizeof(utils::Vertex);
				NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
				NRI.CmdSetDescriptorSet(info.cmdBuffer, 0,
						*m_ConstantBufferDescriptorSet, nullptr);
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

	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "SimpleModelMesh2");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		glm::mat4 m1 = glm::mat4(1.0);
		for (uint32_t meshIndex = 0; meshIndex < m_rootMesh->GetMeshCount(); ++meshIndex) {
			m1 = m_rootMesh->results.at(meshIndex);
			SubMesh *sMesh = m_rootMesh->GetMesh(meshIndex);
			const Material &mat = m_rootMesh->GetMaterial(sMesh->GetMaterialID());
			CBlock block = {};
			block.modelMat = m1;
			block.camPos = vec4(cameraPos, 1.0);
			block.index[0] = mat.m_BaseTexture->GetViewIndex();
			block.index[1] = block.index[2] = block.index[3] = 0.0;
			NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(CBlock));
			NRI.CmdSetIndexBuffer(info.cmdBuffer, *sMesh->m_indexbuffer->GetBuffer(), sMesh->indexOffset,
					nri::IndexType::UINT32);

			nri::VertexBufferDesc vertexBufferDesc = {};
			vertexBufferDesc.buffer = sMesh->m_indexbuffer->GetBuffer();
			vertexBufferDesc.offset = sMesh->vertexOffset;
			vertexBufferDesc.stride = sizeof(utils::Vertex);
			NRI.CmdSetVertexBuffers(info.cmdBuffer, 0, &vertexBufferDesc, 1);
			NRI.CmdSetDescriptorSet(info.cmdBuffer, 0,
					*m_ConstantBufferDescriptorSet, nullptr);

			{
				const nri::Viewport viewport = { 0.0f, 0.0f, 900.f,
					600.f, 0.0f, 1.0f };
				NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

				nri::Rect scissor = { 0, 0, 900, 600 };
				NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
			}
			uint32_t instanceCount = 1;
			NRI.CmdDrawIndexed(info.cmdBuffer, { static_cast<uint32_t>(sMesh->m_indexCount), instanceCount, 0, 0, 0 });
		}
	}
}
