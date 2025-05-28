#include "ssaoCompPass.h"
#include "../renderer.h"
#include "../texture.h"

SSAOCompPass::SSAOCompPass(Renderer *renderer) :
		CommonRenderPass(renderer) {
	AllocGPUMemory();
	BindMemory();
	BuildPipeline();
}
void SSAOCompPass::AllocGPUMemory() {
	auto NRI = *m_NRI;

	// RW SSAO Texture
	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE_STORAGE;
#ifdef HDR_ENABLE
		textureDesc.format = nri::Format::R10_G10_B10_A2_UNORM;
#else
		textureDesc.format = nri::Format::RGBA8_UNORM;
#endif
		textureDesc.width = 900;
		textureDesc.height = 600;
		textureDesc.mipNum = 1;

		nri::Texture2DViewDesc texture2DViewDesc = {};
		texture2DViewDesc.format = textureDesc.format;
		texture2DViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_STORAGE_2D;

		m_SSAOTexture = std::make_shared<Texture>();
		m_SSAOTexture->Create(m_renderer, textureDesc, texture2DViewDesc);
		NRI.SetDebugName(m_SSAOTexture->GetTexture(), "m_SSAOTexture");
	}

	// Rotation Texture
	utils::Texture m_rota_tex_data;

	{
		utils::LoadTexture(utils::GetFullPath("rot_texture.bmp", utils::DataFolder::TEXTURES), m_rota_tex_data);

		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = m_rota_tex_data.GetFormat();
		textureDesc.width = m_rota_tex_data.GetWidth();
		textureDesc.height = m_rota_tex_data.GetHeight();
		textureDesc.mipNum = 1;

		nri::Texture2DViewDesc texViewDesc = {};
		texViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D;
		texViewDesc.format = m_rota_tex_data.GetFormat();

		m_RotationTexture = std::make_shared<Texture>();
		m_RotationTexture->Create(m_renderer, textureDesc, texViewDesc);
		NRI.SetDebugName(m_RotationTexture->GetTexture(), "m_RotationTexture");
	}
	// DpethBuffer SRV
	{
		nri::Texture2DViewDesc texture2DViewDes = { .texture = m_renderer->m_DepthTex, .viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D, .format = nri::Format::D32_SFLOAT };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(texture2DViewDes, m_DepthTextureShaderResource));
	}

	{
		nri::SamplerDesc samplerDesc = {};
		samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE,
			nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
		samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR,
			nri::Filter::LINEAR };
		// samplerDesc.anisotropy = 4;
		samplerDesc.mipMax = 16.0f;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateSampler(*m_renderer->GetRenderDevice(), samplerDesc, m_Sampler));
	}

	nri::TextureSubresourceUploadDesc SSAOSubRes = {};
	SSAOSubRes.rowPitch = 900 * 32;
	SSAOSubRes.slicePitch = SSAOSubRes.rowPitch * 600;
	std::vector<uint8_t> data1(SSAOSubRes.slicePitch, 0u);
	SSAOSubRes.sliceNum = 1;
	SSAOSubRes.slices = data1.data();
	nri::TextureUploadDesc textureData0;
	textureData0.subresources = &SSAOSubRes;
	textureData0.texture = m_SSAOTexture->GetTexture();
	textureData0.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE };
	textureData0.planes = nri::PlaneBits::ALL;

	nri::TextureSubresourceUploadDesc RotationSubRes;
	m_rota_tex_data.GetSubresource(RotationSubRes, 0);
	nri::TextureUploadDesc textureData1;
	textureData1.subresources = &RotationSubRes;
	textureData1.texture = m_RotationTexture->GetTexture();
	textureData1.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
	textureData1.planes = nri::PlaneBits::ALL;

	std::vector<nri::TextureUploadDesc> texUploadDescArray = { textureData0, textureData1 };
	NRI_ABORT_ON_FAILURE(NRI.UploadData(m_renderer->GetRenderQueue(), texUploadDescArray.data(), (uint32_t)texUploadDescArray.size(),
			nullptr,
			0));
}

void SSAOCompPass::BuildPipeline() {
	auto NRI = *m_NRI;

	// SSAO Compute Pipeline
	{
		nri::DescriptorRangeDesc descriptorRangeTexture[3] = {};
		descriptorRangeTexture[0] = { 0, 2, nri::DescriptorType::TEXTURE,
			nri::StageBits::COMPUTE_SHADER };
		descriptorRangeTexture[1] = { 1, 1, nri::DescriptorType::STORAGE_TEXTURE, nri::StageBits::COMPUTE_SHADER };
		descriptorRangeTexture[2] = { 0, 1, nri::DescriptorType::SAMPLER, nri::StageBits::COMPUTE_SHADER };

		nri::DescriptorSetDesc descriptorSetDescs[] = {
			{ 1, descriptorRangeTexture, 3 },
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
				m_SSAOPipelineLayout));
		utils::ShaderCodeStorage shaderCodeStorage;
		nri::ComputePipelineDesc computePipelineDesc = {};
		computePipelineDesc.pipelineLayout = m_SSAOPipelineLayout;
		computePipelineDesc.shader = utils::LoadShader(nri::GraphicsAPI::D3D12, "ssao.cs", shaderCodeStorage);
		NRI_ABORT_ON_FAILURE(NRI.CreateComputePipeline(*m_renderer->GetRenderDevice(), computePipelineDesc, m_SSAOPipeline));
	}

	// Descriptor Set
	{
		NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(m_renderer->GetDescriptorPool(), *m_SSAOPipelineLayout, 0,
				&m_SSAOTextureDescriptorSet, 1, 0));
		NRI.SetDebugName(m_SSAOTextureDescriptorSet, "m_SSAOTextureDescriptorSet");

		std::vector<nri::Descriptor *> ssaoTexView = { m_SSAOTexture->GetView(), m_DepthTextureShaderResource };
		nri::Descriptor *rotationTexView = m_RotationTexture->GetView();
		nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[3] = {};
		descriptorRangeUpdateDescs[0].descriptorNum = 2;
		descriptorRangeUpdateDescs[0].descriptors = ssaoTexView.data();

		descriptorRangeUpdateDescs[1].descriptorNum = 1;
		descriptorRangeUpdateDescs[1].descriptors = &rotationTexView;

		descriptorRangeUpdateDescs[2].descriptorNum = 1;
		descriptorRangeUpdateDescs[2].descriptors = &m_Sampler;

		NRI.UpdateDescriptorRanges(*m_SSAOTextureDescriptorSet, 0,
				helper::GetCountOf(descriptorRangeUpdateDescs),
				descriptorRangeUpdateDescs);
	}
}

void SSAOCompPass::BindMemory() {
	auto NRI = *m_NRI;
}

void SSAOCompPass::Render(struct RenderInfo &info, Camera &camera) {
	auto NRI = *m_NRI;
	{
		helper::Annotation annotation(NRI, info.cmdBuffer, "SSAO Comp Pass");
		NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_SSAOPipelineLayout);
		NRI.CmdSetPipeline(info.cmdBuffer, *m_SSAOPipeline);
		PushConstants block = {};
		block.texOut = 1007,
		block.texDepth = 1008,
		block.texRotation = 1009,
		block.smpl = m_renderer->testVec.w,
		block.zNear = 0.01f,
		block.zFar = 200.0f,
		block.radius = 0.03f,
		block.attScale = 0.95f,
		block.distScale = 1.7f,

		NRI.CmdSetRootConstants(info.cmdBuffer, 0, &block, sizeof(PushConstants));
		NRI.CmdDispatch(info.cmdBuffer, { 900 / 16 + 1, 600 / 16 + 1, 1 });
	}
}