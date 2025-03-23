#include "instanceMeshPass.h"
#include "../renderer.h"
#include "NRIDescs.h"

InstanceMeshPass::InstanceMeshPass(Renderer *renderer) :
		m_renderer(renderer) {
	m_NRI = &m_renderer->GetNRI();
	auto NRI = *m_NRI;

	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_renderer->GetRenderDevice());
	utils::ShaderCodeStorage shaderCodeStorage;
	{
		nri::DescriptorRangeDesc descriptorRangeConstant[1];
		descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER,
			nri::StageBits::ALL };

		nri::DescriptorRangeDesc descriptorRangeTexture[3];
		descriptorRangeTexture[0] = { 0, 2, nri::DescriptorType::TEXTURE,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[1] = { 0, 1, nri::DescriptorType::SAMPLER,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[2] = { 0, 1, nri::DescriptorType::STRUCTURED_BUFFER, nri::StageBits::VERTEX_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeConstant,
					helper::GetCountOf(descriptorRangeConstant) },
			{ 1, descriptorRangeTexture, helper::GetCountOf(descriptorRangeTexture) },
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(glm::vec4),
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
		vertexStreamDesc.stride = sizeof(Vertex);

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
		colorAttachmentDesc.blendEnabled = true;
		colorAttachmentDesc.colorBlend = { nri::BlendFactor::SRC_ALPHA,
			nri::BlendFactor::ONE_MINUS_SRC_ALPHA,
			nri::BlendFunc::ADD };

		nri::DepthAttachmentDesc depthAttachmentDesc = {};
		depthAttachmentDesc.write = true;
		depthAttachmentDesc.compareFunc = nri::CompareFunc::LESS_EQUAL;
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

	// Load Scene Mesh
	const aiScene *scene =
			aiImportFile("data/rubber_duck/scene.gltf",
					aiProcess_Triangulate | aiProcess_MakeLeftHanded);
	if (!scene || !scene->HasMeshes()) {
		printf("Unable to load data/rubber_duck/scene.gltf\n");
		exit(255);
	}

	// Load texture
	utils::Texture texture;
	std::string path =
			utils::GetFullPath("Duck_baseColor.png", utils::DataFolder::TEXTURES);
	if (!utils::LoadTexture(path, texture)) {
		printf("Can not found this texture %s", path.c_str());
	}

	tinyddsloader::DDSFile ddsImage;
	path = utils::GetFullPath("test.dds", utils::DataFolder::TEXTURES);
	ddsImage.Load(path.c_str());

	// GPU Resource
	const uint32_t constantBufferSize = helper::Align((uint32_t)sizeof(ConstantBufferLayout),
			deviceDesc.constantBufferOffsetAlignment);

	const aiMesh *mesh = scene->mMeshes[0];
	std::vector<Vertex> positions;
	std::vector<uint32_t> indices;
	for (unsigned int i = 0; i != mesh->mNumVertices; i++) {
		const aiVector3D v = mesh->mVertices[i];
		const aiVector3D uv0 = mesh->mTextureCoords[0][i];
		const aiVector3D n = mesh->mNormals[i];
		positions.push_back({ vec3(v.x, v.y, v.z), vec2(uv0.x, uv0.y), vec3(n.x, n.y, n.z) });
	}

	for (unsigned int i = 0; i != mesh->mNumFaces; i++) {
		for (int j = 0; j != 3; j++) {
			indices.push_back(mesh->mFaces[i].mIndices[j]);
		}
	}
	m_IndexCount = indices.size();
	const uint64_t indexDataSize = helper::GetByteSizeOf(indices);
	const uint64_t indexDataAlignedSize = helper::Align(indexDataSize, 32);
	const uint64_t vertexDataSize = helper::GetByteSizeOf(positions);

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = texture.GetFormat();
		textureDesc.width = texture.GetWidth();
		textureDesc.height = texture.GetHeight();
		textureDesc.mipNum = texture.GetMipNum();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_Texture));
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = nri::Format::BC7_RGBA_UNORM;
		textureDesc.width = ddsImage.GetWidth();
		textureDesc.height = ddsImage.GetHeight();
		textureDesc.mipNum = 0;
		textureDesc.layerNum = ddsImage.GetArraySize();
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_CubemapTexture));
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
	std::vector<nri::Texture *> textureArray = { m_Texture, m_CubemapTexture };
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
			m_Texture, nri::Texture2DViewType::SHADER_RESOURCE_2D,
			texture.GetFormat()
		};
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDesc, m_TextureShaderResource));
	}

	{
		nri::Texture2DViewDesc textureViewDesc = { .texture = m_CubemapTexture, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_CUBE, .format = nri::Format::BC7_RGBA_UNORM };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(textureViewDesc, m_CubemapTextureShaderResource));
	}
}