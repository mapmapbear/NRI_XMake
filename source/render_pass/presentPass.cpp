#include "presentPass.h"
#include "../renderer.h"
#include <stddef.h>

PresentPass::PresentPass(Renderer *renderer, nri::Texture *colorRT, nri::SwapChain *swapchain) :
		CommonRenderPass(renderer) {
	auto NRI = *m_NRI;
	m_colorRT = colorRT;
	BindMemory();
	BuildPipeline();
}

void PresentPass::AllocGPUMemory() {
}

void PresentPass::BindMemory() {
	auto NRI = *m_NRI;
	// Descriptors
	{
		nri::TextureDesc colorTexDesc = NRI.GetTextureDesc(*m_colorRT);
		nri::Texture2DViewDesc textureViewDesc = { .texture = m_colorRT, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = colorTexDesc.format };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(textureViewDesc, m_ColorRTShaderResource));
	}

	{
		nri::SamplerDesc samplerDesc = {};
		samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_BORDER,
			nri::AddressMode::CLAMP_TO_BORDER, nri::AddressMode::CLAMP_TO_BORDER };

		samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR,
			nri::Filter::LINEAR };
		samplerDesc.anisotropy = 4;
		samplerDesc.mipMax = 16.0f;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateSampler(*m_renderer->GetRenderDevice(), samplerDesc, m_Sampler));
	}
}

void PresentPass::BuildPipeline() {
	auto NRI = *m_NRI;
	utils::ShaderCodeStorage shaderCodeStorage;
	{
		nri::DescriptorRangeDesc descriptorRangeTexture[2];
		descriptorRangeTexture[0] = { 0, 1, nri::DescriptorType::TEXTURE,
			nri::StageBits::FRAGMENT_SHADER };
		descriptorRangeTexture[1] = { 0, 1, nri::DescriptorType::SAMPLER,
			nri::StageBits::FRAGMENT_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 0, descriptorRangeTexture, helper::GetCountOf(descriptorRangeTexture) },
		};

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.descriptorSetNum =
				helper::GetCountOf(descriptorSetDescs);
		pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_graphicsPipelineLayout));

		nri::InputAssemblyDesc inputAssemblyDesc = {};
		inputAssemblyDesc.topology = nri::Topology::TRIANGLE_LIST;

		nri::RasterizationDesc rasterizationDesc = {};
		rasterizationDesc.fillMode = nri::FillMode::SOLID;
		rasterizationDesc.cullMode = nri::CullMode::NONE;

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
		// outputMergerDesc.depth = depthAttachmentDesc;
		// outputMergerDesc.depthStencilFormat = nri::Format::D32_SFLOAT;
		utils::ShaderCodeStorage shaderCodeStorage;
		nri::ShaderDesc shaderStages[] = {
			utils::LoadShader(nri::GraphicsAPI::D3D12,
					"present.vs", shaderCodeStorage),
			utils::LoadShader(nri::GraphicsAPI::D3D12, "present.fs",
					shaderCodeStorage),
		};

		nri::GraphicsPipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.pipelineLayout = m_graphicsPipelineLayout;
		graphicsPipelineDesc.vertexInput = nullptr;
		graphicsPipelineDesc.inputAssembly = inputAssemblyDesc;
		graphicsPipelineDesc.rasterization = rasterizationDesc;
		graphicsPipelineDesc.outputMerger = outputMergerDesc;
		graphicsPipelineDesc.shaders = shaderStages;
		graphicsPipelineDesc.shaderNum = helper::GetCountOf(shaderStages);

		NRI_ABORT_ON_FAILURE(NRI.CreateGraphicsPipeline(
				*m_renderer->GetRenderDevice(), graphicsPipelineDesc, m_graphicsPipeline));
	}

	// Descriptor Set
	{
		NRI_ABORT_ON_FAILURE(
				NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_graphicsPipelineLayout, 0,
						&m_presentTextureDescriptorSet, 1, 0));
		NRI.SetDebugName(m_presentTextureDescriptorSet, "presentDescriptorSet");

		std::vector<nri::Descriptor *> shaderResoruceViewArray = { m_ColorRTShaderResource };

		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = shaderResoruceViewArray.size();
		descriptorRangeUpdateDescs[0].descriptors = shaderResoruceViewArray.data();

		descriptorRangeUpdateDescs[1].descriptorNum = 1;
		descriptorRangeUpdateDescs[1].descriptors = &m_Sampler;

		NRI.UpdateDescriptorRanges(*m_presentTextureDescriptorSet, 0,
				helper::GetCountOf(descriptorRangeUpdateDescs),
				descriptorRangeUpdateDescs);
	}
}

void PresentPass::Render(RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;
	nri::CommandBuffer &commandBuffer = info.cmdBuffer;
	{
		helper::Annotation annotation(NRI, commandBuffer, "Present Pass");
		NRI.CmdSetPipelineLayout(commandBuffer, *m_graphicsPipelineLayout);
		NRI.CmdSetPipeline(commandBuffer, *m_graphicsPipeline);
		NRI.CmdSetDescriptorSet(commandBuffer, 0, *m_presentTextureDescriptorSet,
				nullptr);
		{
			const nri::Viewport viewport = { 0.0f, 0.0f, (float)900.f,
				(float)600.f, 0.0f, 1.0f };
			NRI.CmdSetViewports(commandBuffer, &viewport, 1);

			nri::Rect scissor = { 0, 0, 900, 600 };
			NRI.CmdSetScissors(commandBuffer, &scissor, 1);
		}
		NRI.CmdDraw(commandBuffer, { 3, 1, 0, 0 });
	}
}
