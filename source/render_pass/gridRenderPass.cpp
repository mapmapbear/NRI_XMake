#include "gridRenderPass.h"
#include "../renderer.h"
#include "Camera.h"
#include "NRIDescs.h"

#ifndef HDR_ENABLE
#define HDR_ENABLE
#endif

GridRenderPass::GridRenderPass(Renderer *renderer) :
		CommonRenderPass(renderer) {
	BuildPipeline();
}

void GridRenderPass::AllocGPUMemory() {
}

void GridRenderPass::BindMemory() {
}

void GridRenderPass::BuildPipeline() {
	auto NRI = *m_NRI;
	utils::ShaderCodeStorage shaderCodeStorage;
	{
		struct bindRoot {
			glm::mat4 a;
			vec4 b;
			vec4 c;
		};
		nri::RootConstantDesc rootConstant = { 0, sizeof(bindRoot),
			nri::StageBits::VERTEX_SHADER };

		nri::PipelineLayoutDesc pipelineLayoutDesc = {};
		pipelineLayoutDesc.descriptorSetNum = 0;
		pipelineLayoutDesc.descriptorSets = nullptr;
		pipelineLayoutDesc.rootConstants = &rootConstant;
		pipelineLayoutDesc.rootConstantNum = 1;
		pipelineLayoutDesc.shaderStages =
				nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

		NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_renderer->GetRenderDevice(), pipelineLayoutDesc,
				m_GridPipelineLayout));

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
		outputMergerDesc.depthStencilFormat = nri::Format::D32_SFLOAT;

		nri::ShaderDesc shaderStages[] = {
			utils::LoadShader(nri::GraphicsAPI::D3D12,
					"grid.vs", shaderCodeStorage),
			utils::LoadShader(nri::GraphicsAPI::D3D12, "grid.fs",
					shaderCodeStorage),
		};

		nri::GraphicsPipelineDesc graphicsPipelineDesc;
		graphicsPipelineDesc.pipelineLayout = m_GridPipelineLayout;
		graphicsPipelineDesc.vertexInput = nullptr;
		graphicsPipelineDesc.inputAssembly = inputAssemblyDesc;
		graphicsPipelineDesc.rasterization = rasterizationDesc;
		graphicsPipelineDesc.outputMerger = outputMergerDesc;
		graphicsPipelineDesc.shaders = shaderStages;
		graphicsPipelineDesc.shaderNum = helper::GetCountOf(shaderStages);
		graphicsPipelineDesc.multisample = nullptr;

		NRI_ABORT_ON_FAILURE(NRI.CreateGraphicsPipeline(
				*m_renderer->GetRenderDevice(), graphicsPipelineDesc, m_GridPipeline));
	}
}
void GridRenderPass::Render(RenderInfo &info, Camera1 &camera) {
	auto NRI = *m_NRI;
	helper::Annotation annotation(NRI, info.cmdBuffer, "GridTest");
	NRI.CmdSetPipelineLayout(info.cmdBuffer, *m_GridPipelineLayout);
	NRI.CmdSetPipeline(info.cmdBuffer, *m_GridPipeline);
	struct {
		mat4 mvp;
		vec4 camPos;
		vec4 origin;
	} params = {
		.mvp = camera.matrices.perspective * camera.matrices.view,
		.camPos = vec4(camera.position, 1.0),
		.origin = vec4(0.0)
	};
	NRI.CmdSetRootConstants(info.cmdBuffer, 0, &params, sizeof(params));
	{
		const nri::Viewport viewport = { 0.0f, 0.0f, (float)m_renderer->m_OutputResolution.first,
			(float)m_renderer->m_OutputResolution.second, 0.0f, 1.0f };
		NRI.CmdSetViewports(info.cmdBuffer, &viewport, 1);

		nri::Rect scissor = { 0, 0, (uint16_t)m_renderer->m_OutputResolution.first, (uint16_t)m_renderer->m_OutputResolution.second };
		NRI.CmdSetScissors(info.cmdBuffer, &scissor, 1);
	}
	NRI.CmdDraw(info.cmdBuffer, { 6, 1, 0, 0 });
}