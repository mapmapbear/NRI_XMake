#include "skyRenderPass.h"
#include "../renderer.h"

#include "tinyddsloader.h"

SkyRenderPass::SkyRenderPass(Renderer *renderer) :
		m_renderer(renderer) {
	m_NRI = &m_renderer->GetNRI();
	auto NRI = *m_NRI;

	// SKyBox Pipeline
	{
		nri::DescriptorRangeDesc descriptorRangeTexture[2];
		descriptorRangeTexture[0] = { 0, 1, nri::DescriptorType::TEXTURE,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[1] = { 0, 1, nri::DescriptorType::SAMPLER,
			nri::StageBits::FRAGMENT_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 1, descriptorRangeTexture, helper::GetCountOf(descriptorRangeTexture) },
		};

		nri::RootConstantDesc rootConstant = { 1, sizeof(vec4),
			nri::StageBits::FRAGMENT_SHADER };

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.descriptorSetNum =
				helper::GetCountOf(descriptorSetDescs);
		pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
		pipelineLayoutDesc.rootConstants = &rootConstant;
		pipelineLayoutDesc.rootConstantNum = 1;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_SkyPipelineLayout));

		nri::InputAssemblyDesc inputAssemblyDesc = {};
		inputAssemblyDesc.topology = nri::Topology::TRIANGLE_LIST;

		nri::RasterizationDesc rasterizationDesc = {};
		rasterizationDesc.fillMode = nri::FillMode::SOLID;
		rasterizationDesc.cullMode = nri::CullMode::NONE;

		nri::ColorAttachmentDesc colorAttachmentDesc = {};
		colorAttachmentDesc.format = nri::Format::RGBA8_SNORM;
		colorAttachmentDesc.colorWriteMask = nri::ColorWriteBits::RGBA;
		colorAttachmentDesc.blendEnabled = true;
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
		outputMergerDesc.depth = depthAttachmentDesc;
		outputMergerDesc.depthStencilFormat = nri::Format::D16_UNORM;
		utils::ShaderCodeStorage shaderCodeStorage;
		nri::ShaderDesc shaderStages[] = {
			utils::LoadShader(nri::GraphicsAPI::D3D12,
					"skybox.vs", shaderCodeStorage),
			utils::LoadShader(nri::GraphicsAPI::D3D12, "skybox.fs",
					shaderCodeStorage),
		};

		nri::GraphicsPipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.pipelineLayout = m_SkyPipelineLayout;
		graphicsPipelineDesc.vertexInput = nullptr;
		graphicsPipelineDesc.inputAssembly = inputAssemblyDesc;
		graphicsPipelineDesc.rasterization = rasterizationDesc;
		graphicsPipelineDesc.outputMerger = outputMergerDesc;
		graphicsPipelineDesc.shaders = shaderStages;
		graphicsPipelineDesc.shaderNum = helper::GetCountOf(shaderStages);

		NRI_ABORT_ON_FAILURE(NRI.CreateGraphicsPipeline(
				*m_renderer->GetRenderDevice(), graphicsPipelineDesc, m_SkyPipeline));
	}

	tinyddsloader::DDSFile ddsImage;
	std::string path = utils::GetFullPath("barcelona.dds", utils::DataFolder::TEXTURES);
	ddsImage.Load(path.c_str());

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = nri::Format::BC7_RGBA_UNORM;
		textureDesc.width = ddsImage.GetWidth();
		textureDesc.height = ddsImage.GetHeight();
		textureDesc.mipNum = ddsImage.GetMipCount();

		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_renderer->GetRenderDevice(), textureDesc, m_HDRTexture));
	}

	std::vector<nri::Texture *> textureArray = { m_HDRTexture };
	nri::ResourceGroupDesc resourceGroupDesc = {};
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.textureNum = textureArray.size();
	resourceGroupDesc.textures = textureArray.data();

	m_MemoryAllocations.resize(
			1 + NRI.CalculateAllocationNumber(*m_renderer->GetRenderDevice(), resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_renderer->GetRenderDevice(), resourceGroupDesc, m_MemoryAllocations.data() + 1));

	// Descriptors
	{
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

	{
		nri::Texture2DViewDesc textureViewDesc = { .texture = m_HDRTexture, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = nri::Format::BC7_RGBA_UNORM };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(textureViewDesc, m_HDRTextureShaderResource));
	}

	// Descriptor Set
	{
		NRI_ABORT_ON_FAILURE(
				NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_SkyPipelineLayout, 1,
						&m_SkyTextureDescriptorSet, 1, 0));

		std::vector<nri::Descriptor *> shaderResoruceViewArray = { m_HDRTextureShaderResource };

		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = shaderResoruceViewArray.size();
		descriptorRangeUpdateDescs[0].descriptors = shaderResoruceViewArray.data();

		descriptorRangeUpdateDescs[1].descriptorNum = 1;
		descriptorRangeUpdateDescs[1].descriptors = &m_Sampler;

		NRI.UpdateDescriptorRanges(*m_SkyTextureDescriptorSet, 0,
				helper::GetCountOf(descriptorRangeUpdateDescs),
				descriptorRangeUpdateDescs);
	}

	// Upload data
	const tinyddsloader::DDSFile::ImageData *imgData = ddsImage.GetImageData(0, 0);

	nri::TextureSubresourceUploadDesc hdrSubresources;
	hdrSubresources.slices = imgData->m_mem;
	hdrSubresources.sliceNum = 1;
	hdrSubresources.rowPitch = imgData->m_memPitch;
	hdrSubresources.slicePitch = imgData->m_memSlicePitch;

	nri::TextureUploadDesc textureData;
	textureData.subresources = &hdrSubresources;
	textureData.texture = m_HDRTexture;
	textureData.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
	textureData.planes = nri::PlaneBits::ALL;
	std::vector<nri::TextureUploadDesc> texUploadDescArray = { textureData };
	NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), texUploadDescArray.data(), texUploadDescArray.size(),
			nullptr,
			0));
}

void SkyRenderPass::Render(RenderInfo &info) {
	auto NRI = *m_NRI;
	nri::CommandBuffer &commandBuffer = info.cmdBuffer;
	{
		helper::Annotation annotation(NRI, commandBuffer, "SkyBoxTEST");
		NRI.CmdSetPipelineLayout(commandBuffer, *m_SkyPipelineLayout);
		NRI.CmdSetPipeline(commandBuffer, *m_SkyPipeline);
		// NRI.CmdSetRootConstants(*commandBuffer, 0, &skyParams, sizeof(vec4));
		NRI.CmdSetDescriptorSet(commandBuffer, 0, *m_SkyTextureDescriptorSet,
				nullptr);
		{
			const nri::Viewport viewport = { 0.0f, 0.0f, (float)200,
				(float)90, 0.0f, 1.0f };
			NRI.CmdSetViewports(commandBuffer, &viewport, 1);

			nri::Rect scissor = { 0, 0, 200, 90 };
			NRI.CmdSetScissors(commandBuffer, &scissor, 1);
		}
		NRI.CmdDraw(commandBuffer, { 3, 1, 0, 0 });
	}
}