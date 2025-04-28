#include "instanceMeshPass.h"
#include "../renderer.h"
#include "NRIDescs.h"
#include "assimp/scene.h"
#include "assimp/vector3.h"
#include <format>

InstanceMeshPass::InstanceMeshPass(Renderer *renderer) :
		CommonRenderPass(renderer) {
	m_NRI = &m_renderer->GetNRI();
	auto NRI = *m_NRI;
	AllocGPUMemory();
	BindMemory();
	BuildPipeline();
}

void InstanceMeshPass::AllocGPUMemory() {
	auto NRI = *m_NRI;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());

	const aiScene *scene = aiImportFile("data/DamagedHelmet/glTF/DamagedHelmet.gltf",
			aiProcess_Triangulate | aiProcess_MakeLeftHanded);
	if (!scene || !scene->HasMeshes()) {
		printf("Unable to load data/rubber_duck/scene.gltf\n");
		exit(255);
	}

	// Load texture

	std::string path =
			utils::GetFullPath("DamagedHelmet/glTF/Default_albedo.dds", utils::DataFolder::ROOT);
	if (!utils::LoadTexture(path, m_texture_albedo_data, true)) {
		printf("Can not found this texture %s", path.c_str());
	}

	path = utils::GetFullPath("DamagedHelmet/glTF/Default_normal.dds", utils::DataFolder::ROOT);
	if (!utils::LoadTexture(path, m_texture_normal_data, true)) {
		printf("Can not found this texture %s", path.c_str());
	}

	path = utils::GetFullPath("DamagedHelmet/glTF/Default_metalRoughness.dds", utils::DataFolder::ROOT);
	if (!utils::LoadTexture(path, m_texture_mr_data, true)) {
		printf("Can not found this texture %s", path.c_str());
	}

	path = utils::GetFullPath("DamagedHelmet/glTF/Default_AO.dds", utils::DataFolder::ROOT);
	if (!utils::LoadTexture(path, m_texture_ao_data, true)) {
		printf("Can not found this texture %s", path.c_str());
	}

	path = utils::GetFullPath("DamagedHelmet/glTF/Default_emissive.dds", utils::DataFolder::ROOT);
	if (!utils::LoadTexture(path, m_texture_emissive_data, true)) {
		printf("Can not found this texture %s", path.c_str());
	}

	// GPU Resource
	const uint32_t constantBufferSize = helper::Align((uint32_t)sizeof(ConstantBufferLayout),
			deviceDesc.memoryAlignment.constantBufferOffset);

	const aiMesh *mesh = scene->mMeshes[0];

	for (unsigned int i = 0; i != mesh->mNumVertices; i++) {
		const aiVector3D v = mesh->mVertices[i];
		const aiVector3D uv0 = mesh->mTextureCoords[0][i];
		const aiVector3D n = mesh->mNormals[i];
		// const aiVector3D t = mesh->mTangents[i];
		// const aiVector3D bt = mesh->mBitangents[i];
		m_positions.push_back({ vec3(v.x, v.y, v.z), vec2(uv0.x, uv0.y), vec3(n.x, n.y, n.z), vec3(0.0), vec3(0.0) });
	}

	for (unsigned int i = 0; i != mesh->mNumFaces; i++) {
		for (int j = 0; j != 3; j++) {
			m_indices.push_back(mesh->mFaces[i].mIndices[j]);
		}
	}
	m_IndexCount = m_indices.size();
	const uint64_t indexDataSize = helper::GetByteSizeOf(m_indices);
	const uint64_t indexDataAlignedSize = helper::Align(indexDataSize, 32);
	const uint64_t vertexDataSize = helper::GetByteSizeOf(m_positions);

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

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = m_texture_mr_data.GetFormat(true);
		textureDesc.width = m_texture_mr_data.GetWidth();
		textureDesc.height = m_texture_mr_data.GetHeight();
		textureDesc.mipNum = 1;
		textureDesc.depth = m_texture_mr_data.GetDepth();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_texture_mr));
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = m_texture_ao_data.GetFormat(true);
		textureDesc.width = m_texture_ao_data.GetWidth();
		textureDesc.height = m_texture_ao_data.GetHeight();
		textureDesc.mipNum = 1;
		textureDesc.depth = m_texture_ao_data.GetDepth();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_texture_ao));
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = m_texture_emissive_data.GetFormat(true);
		textureDesc.width = m_texture_emissive_data.GetWidth();
		textureDesc.height = m_texture_emissive_data.GetHeight();
		textureDesc.mipNum = 1;
		textureDesc.depth = m_texture_emissive_data.GetDepth();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_texture_emissive));
	}

	{
		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = constantBufferSize * BUFFERED_FRAME_MAX_NUM;
		bufferDesc.usage = nri::BufferUsageBits::CONSTANT_BUFFER;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateBuffer(*m_renderer->GetRenderDevice(), bufferDesc, m_ConstantBuffer));
	}

	{ // Geometry buffer1（duck)
		nri::BufferDesc bufferDesc = {};
		bufferDesc.size = indexDataAlignedSize + vertexDataSize;
		bufferDesc.usage = nri::BufferUsageBits::VERTEX_BUFFER |
				nri::BufferUsageBits::INDEX_BUFFER;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateBuffer(*m_renderer->GetRenderDevice(), bufferDesc, m_GeometryBuffer));
		m_GeometryOffset = indexDataAlignedSize;
	}
}

void InstanceMeshPass::BindMemory() {
	auto NRI = *m_NRI;

	m_IndexCount = m_indices.size();
	const uint64_t indexDataSize = helper::GetByteSizeOf(m_indices);
	const uint64_t indexDataAlignedSize = helper::Align(indexDataSize, 32);
	const uint64_t vertexDataSize = helper::GetByteSizeOf(m_positions);

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
	std::vector<nri::Texture *> textureArray = { m_texture_albedo, m_texture_normal, m_texture_mr, m_texture_ao, m_texture_emissive }; //, m_CubemapTexture };
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.bufferNum = bufferArray.size();
	resourceGroupDesc.buffers = bufferArray.data();
	resourceGroupDesc.textureNum = textureArray.size();
	resourceGroupDesc.textures = textureArray.data();

	m_MemoryAllocations.resize(
			1 + NRI.CalculateAllocationNumber(*m_renderer->GetRenderDevice(), resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_renderer->GetRenderDevice(), resourceGroupDesc, m_MemoryAllocations.data() + 1));

	// Descriptors
	{ // Read-only texture
		nri::Texture2DViewDesc texture2DViewDesc = {
			m_texture_albedo, nri::Texture2DViewType::SHADER_RESOURCE_2D,
			m_texture_albedo_data.GetFormat()
		};
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDesc, m_texture_albedo_view));
		SPDLOG_INFO("texOffset= {}\n", m_renderer->texViewOffset++);
	}

	{ // Read-only texture
		nri::Texture2DViewDesc texture2DViewDesc = {
			m_texture_normal, nri::Texture2DViewType::SHADER_RESOURCE_2D,
			m_texture_normal_data.GetFormat()
		};
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDesc, m_texture_normal_view));
		SPDLOG_INFO("texOffset= {}\n", m_renderer->texViewOffset++);
	}

	{ // Read-only texture
		nri::Texture2DViewDesc texture2DViewDesc = {
			m_texture_mr, nri::Texture2DViewType::SHADER_RESOURCE_2D,
			m_texture_mr_data.GetFormat()
		};
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDesc, m_texture_mr_view));
		SPDLOG_INFO("texOffset= {}\n", m_renderer->texViewOffset++);
	}

	{ // Read-only texture
		nri::Texture2DViewDesc texture2DViewDesc = {
			m_texture_ao, nri::Texture2DViewType::SHADER_RESOURCE_2D,
			m_texture_ao_data.GetFormat()
		};
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDesc, m_texture_ao_view));
		SPDLOG_INFO("texOffset= {}\n", m_renderer->texViewOffset++);
	}

	{ // Read-only texture
		nri::Texture2DViewDesc texture2DViewDesc = {
			m_texture_emissive, nri::Texture2DViewType::SHADER_RESOURCE_2D,
			m_texture_emissive_data.GetFormat()
		};
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDesc, m_texture_emissive_view));
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
		std::vector<uint8_t> geometryBufferData(indexDataAlignedSize +
				vertexDataSize);
		memcpy(&geometryBufferData[0], m_indices.data(), indexDataSize);
		memcpy(&geometryBufferData[indexDataAlignedSize], m_positions.data(),
				vertexDataSize);

		nri::BufferUploadDesc bufferData = {};
		bufferData.buffer = m_GeometryBuffer;
		bufferData.data = &geometryBufferData[0];
		bufferData.dataSize = geometryBufferData.size();
		bufferData.after = { nri::AccessBits::INDEX_BUFFER |
			nri::AccessBits::VERTEX_BUFFER };
		std::vector<nri::BufferUploadDesc> uploadDescArray = { bufferData };

		std::vector<nri::TextureUploadDesc> texUploadDescArray = {};
		std::array<nri::TextureSubresourceUploadDesc, 16> subresources;
		// std::vector<utils::Texture> tex_data_array = { m_texture_albedo_data, m_texture_normal_data, m_texture_mr_data, m_texture_ao_data, m_texture_emissive_data };
		// std::vector<nri::Texture *> tex_array = { m_texture_albedo, m_texture_normal, m_texture_mr, m_texture_ao, m_texture_emissive };
		// for (size_t i = 0; i < tex_data_array.size(); i++) {
		// 	auto &tex_data = tex_data_array[i];
		// 	std::vector<nri::TextureSubresourceUploadDesc> subDataArr = {};
		// 	for (uint32_t mip = 0; mip < tex_data.GetMipNum(); mip++) {
		// 		auto imgData = tex_data.data.GetImageData(mip, 0);
		// 		nri::TextureSubresourceUploadDesc sub_resource_desc = {};
		// 		sub_resource_desc.slices = imgData->m_mem;
		// 		sub_resource_desc.sliceNum = 1;
		// 		sub_resource_desc.rowPitch = imgData->m_memPitch;
		// 		sub_resource_desc.slicePitch = imgData->m_memSlicePitch;
		// 		subDataArr.emplace_back(sub_resource_desc);
		// 	}

		// 	nri::TextureUploadDesc textureData;
		// 	textureData.subresources = subDataArr.data();
		// 	textureData.texture = tex_array[i];
		// 	textureData.after = { nri::AccessBits::SHADER_RESOURCE,
		// 		nri::Layout::SHADER_RESOURCE };
		// 	textureData.planes = nri::PlaneBits::ALL;

		// 	texUploadDescArray.push_back(textureData);
		// }

		{
			auto imgData = m_texture_albedo_data.data.GetImageData(0, 0);
			nri::TextureSubresourceUploadDesc sub_resource_desc = {};
			sub_resource_desc.slices = imgData->m_mem;
			sub_resource_desc.sliceNum = 1;
			sub_resource_desc.rowPitch = imgData->m_memPitch;
			sub_resource_desc.slicePitch = imgData->m_memSlicePitch;

			nri::TextureUploadDesc textureData;
			textureData.subresources = &sub_resource_desc;
			textureData.texture = m_texture_albedo;
			textureData.after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			textureData.planes = nri::PlaneBits::ALL;
			texUploadDescArray.push_back(textureData);
		}

		{
			auto imgData = m_texture_normal_data.data.GetImageData(0, 0);
			nri::TextureSubresourceUploadDesc sub_resource_desc = {};
			sub_resource_desc.slices = imgData->m_mem;
			sub_resource_desc.sliceNum = 1;
			sub_resource_desc.rowPitch = imgData->m_memPitch;
			sub_resource_desc.slicePitch = imgData->m_memSlicePitch;

			nri::TextureUploadDesc textureData;
			textureData.subresources = &sub_resource_desc;
			textureData.texture = m_texture_normal;
			textureData.after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			textureData.planes = nri::PlaneBits::ALL;
			texUploadDescArray.push_back(textureData);
		}

		{
			auto imgData = m_texture_mr_data.data.GetImageData(0, 0);
			nri::TextureSubresourceUploadDesc sub_resource_desc = {};
			sub_resource_desc.slices = imgData->m_mem;
			sub_resource_desc.sliceNum = 1;
			sub_resource_desc.rowPitch = imgData->m_memPitch;
			sub_resource_desc.slicePitch = imgData->m_memSlicePitch;

			nri::TextureUploadDesc textureData;
			textureData.subresources = &sub_resource_desc;
			textureData.texture = m_texture_mr;
			textureData.after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			textureData.planes = nri::PlaneBits::ALL;
			texUploadDescArray.push_back(textureData);
		}

		{
			auto imgData = m_texture_ao_data.data.GetImageData(0, 0);
			nri::TextureSubresourceUploadDesc sub_resource_desc = {};
			sub_resource_desc.slices = imgData->m_mem;
			sub_resource_desc.sliceNum = 1;
			sub_resource_desc.rowPitch = imgData->m_memPitch;
			sub_resource_desc.slicePitch = imgData->m_memSlicePitch;

			nri::TextureUploadDesc textureData;
			textureData.subresources = &sub_resource_desc;
			textureData.texture = m_texture_ao;
			textureData.after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			textureData.planes = nri::PlaneBits::ALL;
			texUploadDescArray.push_back(textureData);
		}

		{
			auto imgData = m_texture_emissive_data.data.GetImageData(0, 0);
			nri::TextureSubresourceUploadDesc sub_resource_desc = {};
			sub_resource_desc.slices = imgData->m_mem;
			sub_resource_desc.sliceNum = 1;
			sub_resource_desc.rowPitch = imgData->m_memPitch;
			sub_resource_desc.slicePitch = imgData->m_memSlicePitch;

			nri::TextureUploadDesc textureData;
			textureData.subresources = &sub_resource_desc;
			textureData.texture = m_texture_emissive;
			textureData.after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			textureData.planes = nri::PlaneBits::ALL;
			texUploadDescArray.push_back(textureData);
		}

		NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), texUploadDescArray.data(), texUploadDescArray.size(),
				uploadDescArray.data(),
				uploadDescArray.size()));
	}
}

void InstanceMeshPass::BuildPipeline() {
	auto NRI = *m_NRI;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());

	// Pipeline
	utils::ShaderCodeStorage shaderCodeStorage;
	{
		nri::DescriptorRangeDesc descriptorRangeConstant[1];
		descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER,
			nri::StageBits::ALL };

		nri::DescriptorRangeDesc descriptorRangeTexture[2];
		descriptorRangeTexture[0] = { 0, 5, nri::DescriptorType::TEXTURE,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[1] = { 0, 1, nri::DescriptorType::SAMPLER,
			nri::StageBits::FRAGMENT_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeConstant,
					helper::GetCountOf(descriptorRangeConstant) },
			{ 1, descriptorRangeTexture, helper::GetCountOf(descriptorRangeTexture) },
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(glm::vec4) + sizeof(uint32_t),
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
		// vertexStreamDesc.stepRate = sizeof(Vertex);

		nri::VertexAttributeDesc vertexAttributeDesc[3] = {};
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
		outputMergerDesc.depthStencilFormat = nri::Format::D16_UNORM;

		nri::ShaderDesc shaderStages[] = {
			utils::LoadShader(deviceDesc.graphicsAPI,
					"simpleMesh.vs", shaderCodeStorage),
			utils::LoadShader(deviceDesc.graphicsAPI, "simpleMesh.fs",
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

		std::vector<nri::Descriptor *> shaderResoruceViewArray = { m_texture_albedo_view, m_texture_normal_view, m_texture_mr_view, m_texture_ao_view, m_texture_emissive_view }; //, m_CubemapTextureShaderResource };

		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = shaderResoruceViewArray.size();
		descriptorRangeUpdateDescs[0].descriptors = shaderResoruceViewArray.data();

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

void InstanceMeshPass::Render(RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;

	ConstantBufferLayout *commonConstants = (ConstantBufferLayout *)NRI.MapBuffer(
			*m_ConstantBuffer, 0,
			sizeof(ConstantBufferLayout));

	const glm::mat4 m1 = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
			glm::vec3(1.0f, 0.f, 0.f));
	const glm::mat4 m2 = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime(),
			glm::vec3(0.0f, 1.f, 0.f));
	glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.8f, 0.0f)) * m2 * m1;
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
		helper::Annotation annotation(NRI, info.cmdBuffer, "SimpleMeshTEST");

		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_PipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_Pipeline);
		struct {
			vec4 camPos;
			uint32_t index;
		} matBlock;
		matBlock.camPos = vec4(cameraPos, 1.0);
		matBlock.index = m_renderer->testIndex;
		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &matBlock, sizeof(glm::vec4) + sizeof(uint32_t));
		NRI.CmdSetIndexBuffer(info.cmdBuffer, *m_GeometryBuffer, 0,
				nri::IndexType::UINT32);

		nri::VertexBufferDesc vertexBufferDesc = {};
		vertexBufferDesc.buffer = m_GeometryBuffer;
		vertexBufferDesc.offset = m_GeometryOffset;
		vertexBufferDesc.stride = sizeof(utils::Vertex);
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
		NRI.CmdDrawIndexed(info.cmdBuffer, { m_IndexCount, instanceCount, 0, 0, 0 });
	}
}
