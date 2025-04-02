// © 2021 NVIDIA Corporation
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/random.hpp"
#include "glm/trigonometric.hpp"
#include "imgui.h"
#include "renderer.h"

// STB
#include "stb_image.h"

// ASSIMP
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>
#include <vector>

#define INSTANCE

constexpr uint32_t VIEW_MASK = 0b11;
constexpr nri::Color32f COLOR_0 = { 1.0f, 1.0f, 0.0f, 1.0f };
constexpr nri::Color32f COLOR_1 = { 0.46f, 0.72f, 0.0f, 1.0f };
struct ConstantBufferLayout {
	glm::mat4 modelMat;
	glm::mat4 viewMat;
	glm::mat4 projectMat;
};

struct Vertex {
	vec3 position;
	vec2 uv;
	vec3 normal;
	Vertex(vec3 pos, vec2 uv, vec3 nor) :
			position(pos), uv(uv), normal(nor) {}
};

static uint32_t g_indexCount = 0;

struct Frame {
	nri::CommandAllocator *commandAllocator;
	nri::CommandBuffer *commandBuffer;
	nri::CommandAllocator *commandAllocatorCompute;
	nri::CommandBuffer *commandBufferCompute;
	nri::Descriptor *constantBufferView;
	nri::DescriptorSet *constantBufferDescriptorSet;
	uint64_t constantBufferViewOffset;
};

class Sample : public SampleBase {
public:
	Sample() {}

	~Sample();

	bool Initialize(nri::GraphicsAPI graphicsAPI) override;
	void PrepareFrame(uint32_t frameIndex) override;
	void RenderFrame(uint32_t frameIndex) override;

private:
	NRIInterface NRI = {};
	nri::Device *m_Device = nullptr;
	nri::Streamer *m_Streamer = nullptr;
	nri::SwapChain *m_SwapChain = nullptr;
	nri::Queue *m_GraphicsQueue = nullptr;
	nri::Queue *m_ComputeQueue = nullptr;
	nri::Fence *m_FrameFence = nullptr;
	nri::Fence *m_ComputeFence = nullptr;
	nri::DescriptorPool *m_DescriptorPool = nullptr;
	nri::PipelineLayout *m_PipelineLayout = nullptr;
	nri::Pipeline *m_Pipeline = nullptr;
	nri::PipelineLayout *m_SkyPipelineLayout = nullptr;
	nri::PipelineLayout *m_GridPipelineLayout = nullptr;
	nri::PipelineLayout *m_ComputePipelineLayout = nullptr;
	nri::Pipeline *m_SkyPipeline = nullptr;
	nri::Pipeline *m_GridPipeline = nullptr;
	nri::Pipeline *m_ComputePipeline = nullptr;
	nri::Pipeline *m_PipelineMultiview = nullptr;
	nri::DescriptorSet *m_TextureDescriptorSet = nullptr;
	nri::DescriptorSet *m_BufferDescriptorSet = nullptr;
	nri::DescriptorSet *m_SkyTextureDescriptorSet = nullptr;
	nri::DescriptorSet *m_ComputeBufferDescriptorSet = nullptr;
	nri::Descriptor *m_TextureShaderResource = nullptr;
	nri::Descriptor *m_HDRTextureShaderResource = nullptr;
	nri::Descriptor *m_CubemapTextureShaderResource = nullptr;
	nri::Descriptor *m_DepthAttachment = nullptr;
	nri::Descriptor *m_ColorAttachment = nullptr;
	nri::Descriptor *m_Sampler = nullptr;
	nri::Descriptor *m_CubeSampler = nullptr;
	nri::Descriptor *m_PosStorageShaderResource = nullptr;
	nri::Descriptor *m_MatrixStorageShaderResource = nullptr;
	nri::Descriptor *m_MatrixStorageBufferSRV = nullptr;
	nri::Buffer *m_ConstantBuffer = nullptr;
	nri::Buffer *m_GeometryBuffer = nullptr;
	nri::Buffer *m_PositionStorageBuffer = nullptr;
	nri::Buffer *m_MatrixStorageBuffer = nullptr;
	nri::Texture *m_Texture = nullptr;
	nri::Texture *m_HDRTexture = nullptr;
	nri::Texture *m_CubemapTexture = nullptr;
	nri::Texture *m_DepthTexture = nullptr;
	nri::Texture *m_ColorTexture = nullptr;

	std::array<Frame, BUFFERED_FRAME_MAX_NUM> m_Frames = {};
	std::vector<BackBuffer> m_SwapChainBuffers;
	std::vector<nri::Memory *> m_MemoryAllocations;

	uint64_t m_GeometryOffset = 0;
	bool m_Multiview = false;
	float m_Transparency = 1.0f;
	float m_Scale = 1.0f;
	float m_Fov = 45.0f;
	vec4 skyParams;

	Renderer *testRenderPtr;
};

Sample::~Sample() {
	NRI.WaitForIdle(*m_GraphicsQueue);
	NRI.WaitForIdle(*m_ComputeQueue);

	for (Frame &frame : m_Frames) {
		NRI.DestroyCommandBuffer(*frame.commandBuffer);
		NRI.DestroyCommandAllocator(*frame.commandAllocator);
		NRI.DestroyCommandBuffer(*frame.commandBufferCompute);
		NRI.DestroyCommandAllocator(*frame.commandAllocatorCompute);
		NRI.DestroyDescriptor(*frame.constantBufferView);
	}

	for (BackBuffer &backBuffer : m_SwapChainBuffers) {
		NRI.DestroyDescriptor(*backBuffer.colorAttachment);
	}

	NRI.DestroyPipeline(*m_Pipeline);
	NRI.DestroyPipeline(*m_SkyPipeline);
	NRI.DestroyPipeline(*m_GridPipeline);
	NRI.DestroyPipeline(*m_ComputePipeline);
	NRI.DestroyPipeline(*m_PipelineMultiview);
	NRI.DestroyPipelineLayout(*m_PipelineLayout);
	NRI.DestroyPipelineLayout(*m_SkyPipelineLayout);
	NRI.DestroyPipelineLayout(*m_GridPipelineLayout);
	NRI.DestroyPipelineLayout(*m_ComputePipelineLayout);
	NRI.DestroyDescriptor(*m_TextureShaderResource);
	NRI.DestroyDescriptor(*m_DepthAttachment);
	NRI.DestroyDescriptor(*m_Sampler);
	NRI.DestroyDescriptor(*m_CubeSampler);
	NRI.DestroyBuffer(*m_ConstantBuffer);
	NRI.DestroyBuffer(*m_GeometryBuffer);
	NRI.DestroyTexture(*m_Texture);
	NRI.DestroyTexture(*m_DepthTexture);
	NRI.DestroyDescriptorPool(*m_DescriptorPool);
	NRI.DestroyFence(*m_FrameFence);
	NRI.DestroySwapChain(*m_SwapChain);
	NRI.DestroyStreamer(*m_Streamer);

	for (nri::Memory *memory : m_MemoryAllocations) {
		NRI.FreeMemory(*memory);
	}

	DestroyUI(NRI);

	nri::nriDestroyDevice(*m_Device);
}

bool Sample::Initialize(nri::GraphicsAPI graphicsAPI) {
	nri::AdapterDesc bestAdapterDesc = {};
	uint32_t adapterDescsNum = 1;
	NRI_ABORT_ON_FAILURE(
			nri::nriEnumerateAdapters(&bestAdapterDesc, adapterDescsNum));

	nri::QueueFamilyDesc queueFamilies[2] = {};
	queueFamilies[0].queueNum = 1;
	queueFamilies[0].queueType = nri::QueueType::GRAPHICS;
	queueFamilies[1].queueNum = 1;
	queueFamilies[1].queueType = nri::QueueType::COMPUTE;

	// Device
	nri::DeviceCreationDesc deviceCreationDesc = {};
	deviceCreationDesc.graphicsAPI = graphicsAPI;
	deviceCreationDesc.queueFamilies = queueFamilies;
	deviceCreationDesc.queueFamilyNum = helper::GetCountOf(queueFamilies);
	deviceCreationDesc.enableGraphicsAPIValidation = true;
	deviceCreationDesc.enableNRIValidation = m_DebugNRI;
	deviceCreationDesc.enableD3D11CommandBufferEmulation =
			D3D11_COMMANDBUFFER_EMULATION;
	deviceCreationDesc.vkBindingOffsets = VK_BINDING_OFFSETS;
	deviceCreationDesc.adapterDesc = &bestAdapterDesc;
	deviceCreationDesc.allocationCallbacks = m_AllocationCallbacks;
	NRI_ABORT_ON_FAILURE(nri::nriCreateDevice(deviceCreationDesc, m_Device));

	// NRI
	NRI_ABORT_ON_FAILURE(nri::nriGetInterface(*m_Device,
			NRI_INTERFACE(nri::CoreInterface),
			(nri::CoreInterface *)&NRI));
	NRI_ABORT_ON_FAILURE(nri::nriGetInterface(*m_Device,
			NRI_INTERFACE(nri::HelperInterface),
			(nri::HelperInterface *)&NRI));
	NRI_ABORT_ON_FAILURE(
			nri::nriGetInterface(*m_Device, NRI_INTERFACE(nri::StreamerInterface),
					(nri::StreamerInterface *)&NRI));
	NRI_ABORT_ON_FAILURE(
			nri::nriGetInterface(*m_Device, NRI_INTERFACE(nri::SwapChainInterface),
					(nri::SwapChainInterface *)&NRI));

	// Create streamer
	nri::StreamerDesc streamerDesc = {};
	streamerDesc.dynamicBufferMemoryLocation = nri::MemoryLocation::HOST_UPLOAD;
	streamerDesc.dynamicBufferUsageBits =
			nri::BufferUsageBits::VERTEX_BUFFER | nri::BufferUsageBits::INDEX_BUFFER;
	streamerDesc.constantBufferMemoryLocation = nri::MemoryLocation::HOST_UPLOAD;
	streamerDesc.frameInFlightNum = BUFFERED_FRAME_MAX_NUM;
	NRI_ABORT_ON_FAILURE(NRI.CreateStreamer(*m_Device, streamerDesc, m_Streamer));

	// Command queue
	NRI_ABORT_ON_FAILURE(NRI.GetQueue(*m_Device, nri::QueueType::GRAPHICS, 0, m_GraphicsQueue));
	NRI.SetDebugName(m_GraphicsQueue, "GraphicsQueue");

	NRI_ABORT_ON_FAILURE(NRI.GetQueue(*m_Device, nri::QueueType::COMPUTE, 0, m_ComputeQueue));
	NRI.SetDebugName(m_ComputeQueue, "ComputeQueue");

	testRenderPtr = new Renderer(NRI, m_Device);

	// Fences
	NRI_ABORT_ON_FAILURE(NRI.CreateFence(*m_Device, 0, m_FrameFence));
	NRI_ABORT_ON_FAILURE(NRI.CreateFence(*m_Device, 0, m_ComputeFence));
	// Swap chain
	nri::Format swapChainFormat;
	{
		nri::SwapChainDesc swapChainDesc = {};
		swapChainDesc.window = GetWindow();
		swapChainDesc.queue = m_GraphicsQueue;
		swapChainDesc.format = nri::SwapChainFormat::BT709_G22_8BIT;
		swapChainDesc.verticalSyncInterval = m_VsyncInterval;
		swapChainDesc.width = (uint16_t)GetWindowResolution().first;
		swapChainDesc.height = (uint16_t)GetWindowResolution().second;
		swapChainDesc.textureNum = SWAP_CHAIN_TEXTURE_NUM;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateSwapChain(*m_Device, swapChainDesc, m_SwapChain));

		uint32_t swapChainTextureNum;
		nri::Texture *const *swapChainTextures =
				NRI.GetSwapChainTextures(*m_SwapChain, swapChainTextureNum);
		swapChainFormat = NRI.GetTextureDesc(*swapChainTextures[0]).format;

		for (uint32_t i = 0; i < swapChainTextureNum; i++) {
			nri::Texture2DViewDesc textureViewDesc = {
				swapChainTextures[i], nri::Texture2DViewType::COLOR_ATTACHMENT,
				swapChainFormat
			};

			nri::Descriptor *colorAttachment;
			NRI_ABORT_ON_FAILURE(
					NRI.CreateTexture2DView(textureViewDesc, colorAttachment));

			BackBuffer backBuffer = { colorAttachment, swapChainTextures[i] };
			m_SwapChainBuffers.push_back(backBuffer);
		}
	}

	// Buffered resources
	for (Frame &frame : m_Frames) {
		NRI_ABORT_ON_FAILURE(
				NRI.CreateCommandAllocator(*m_GraphicsQueue, frame.commandAllocator));
		NRI_ABORT_ON_FAILURE(
				NRI.CreateCommandBuffer(*frame.commandAllocator, frame.commandBuffer));

		NRI_ABORT_ON_FAILURE(
				NRI.CreateCommandAllocator(*m_ComputeQueue, frame.commandAllocatorCompute));
		NRI_ABORT_ON_FAILURE(
				NRI.CreateCommandBuffer(*frame.commandAllocatorCompute, frame.commandBufferCompute));
	}
	testRenderPtr->OnStart(nullptr);
	utils::ShaderCodeStorage shaderCodeStorage;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_Device);

	// // Compute pipeline
	// {
	// 	nri::DescriptorRangeDesc descriptorRangeComp[2];
	// 	descriptorRangeComp[0] = { 0, 1, nri::DescriptorType::STRUCTURED_BUFFER,
	// 		nri::StageBits::COMPUTE_SHADER };
	// 	descriptorRangeComp[1] = { 0, 1, nri::DescriptorType::STORAGE_BUFFER,
	// 		nri::StageBits::COMPUTE_SHADER };

	// 	nri::DescriptorSetDesc descriptorSetDesc = { 0, descriptorRangeComp, 2 };

	// 	struct bindRoot {
	// 		vec4 a;
	// 	};
	// 	nri::RootConstantDesc rootConstant = { 0, sizeof(bindRoot),
	// 		nri::StageBits::COMPUTE_SHADER };

	// 	nri::PipelineLayoutDesc pipelineLayoutDesc = {};
	// 	pipelineLayoutDesc.descriptorSetNum = 1;
	// 	pipelineLayoutDesc.descriptorSets = &descriptorSetDesc;
	// 	pipelineLayoutDesc.rootConstantNum = 1;
	// 	pipelineLayoutDesc.rootConstants = &rootConstant;
	// 	pipelineLayoutDesc.shaderStages = nri::StageBits::COMPUTE_SHADER;
	// 	NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_Device, pipelineLayoutDesc, m_ComputePipelineLayout));
	// 	NRI.SetDebugName(m_ComputePipelineLayout, "Compute Pipeline Layout");
	// 	nri::ComputePipelineDesc computePipelineDesc = {};
	// 	computePipelineDesc.pipelineLayout = m_ComputePipelineLayout;
	// 	computePipelineDesc.shader = utils::LoadShader(nri::GraphicsAPI::D3D12, "instanceGenBuffer.cs", shaderCodeStorage);
	// 	NRI_ABORT_ON_FAILURE(NRI.CreateComputePipeline(*m_Device, computePipelineDesc, m_ComputePipeline));
	// }

	m_DescriptorPool = &testRenderPtr->GetDescriptorPool();

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::COLOR_ATTACHMENT;
		textureDesc.format = nri::Format::RGBA8_UNORM;
		textureDesc.width = (uint16_t)GetWindowResolution().first;
		textureDesc.height = (uint16_t)GetWindowResolution().second;
		textureDesc.mipNum = 1;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_Device, textureDesc, m_ColorTexture));
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::DEPTH_STENCIL_ATTACHMENT;
		textureDesc.format = nri::Format::D16_UNORM;
		textureDesc.width = (uint16_t)GetWindowResolution().first;
		textureDesc.height = (uint16_t)GetWindowResolution().second;
		textureDesc.mipNum = 1;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_Device, textureDesc, m_DepthTexture));
	}

	nri::ResourceGroupDesc resourceGroupDesc = {};
	std::vector<nri::Texture *> textureArray = { m_DepthTexture, m_ColorTexture };
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.textureNum = textureArray.size();
	resourceGroupDesc.textures = textureArray.data();

	m_MemoryAllocations.resize(
			NRI.CalculateAllocationNumber(*m_Device, resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_Device, resourceGroupDesc, m_MemoryAllocations.data()));

	nri::TextureUploadDesc textureData;
	textureData.subresources = nullptr;
	textureData.texture = m_DepthTexture;
	textureData.after = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT };
	textureData.planes = nri::PlaneBits::ALL;

	nri::TextureUploadDesc textureData1;
	textureData1.subresources = nullptr;
	textureData1.texture = m_ColorTexture;
	textureData1.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT };
	textureData1.planes = nri::PlaneBits::ALL;

	std::vector<nri::TextureUploadDesc> texUploadDescArray = { textureData, textureData1 };

	NRI_ABORT_ON_FAILURE(NRI.UploadData(*m_GraphicsQueue, texUploadDescArray.data(), texUploadDescArray.size(),
			nullptr,
			0));

	{
		nri::Texture2DViewDesc textureViewDesc = { .texture = m_DepthTexture, .viewType = nri::Texture2DViewType::DEPTH_STENCIL_ATTACHMENT, .format = nri::Format::D16_UNORM };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(textureViewDesc, m_DepthAttachment));
	}

	{
		nri::Texture2DViewDesc textureViewDesc = { .texture = m_ColorTexture, .viewType = nri::Texture2DViewType::COLOR_ATTACHMENT, .format = nri::Format::RGBA8_UNORM };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(textureViewDesc, m_ColorAttachment));
	}

	testRenderPtr->InitPresentPass(m_ColorTexture, m_SwapChain);

	// User interface
	bool initialized = InitUI(NRI, NRI, *m_Device, swapChainFormat);
	m_Camera.Initialize(glm::vec3(0.0f, 0.0f, -3.5f), glm::vec3(0.0f, 0.0f, 0.0f));
	return initialized;
}

void Sample::PrepareFrame(uint32_t frameIndex) {
	BeginUI();

	ImGui::SetNextWindowPos(ImVec2(30, 30), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(0, 0));
	ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize);
	{
		ImGui::SliderFloat("Transparency", &m_Transparency, 0.0f, 1.0f);
		ImGui::SliderFloat("Scale", &m_Scale, 0.75f, 1.25f);
		ImGui::SliderFloat("Fov", &m_Fov, 20.0f, 120.0f, "%.0f");

		const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_Device);
		ImGui::BeginDisabled(!deviceDesc.isFlexibleMultiviewSupported);
		ImGui::Checkbox("Multiview", &m_Multiview);
		ImGui::EndDisabled();
	}
	ImGui::End();

	ImGui::ShowDemoWindow();

	EndUI(NRI, *m_Streamer);
	NRI.CopyStreamerUpdateRequests(*m_Streamer);

	CameraDesc desc = {};
	desc.aspectRatio = float(GetWindowResolution().first) / float(GetWindowResolution().second);
	desc.horizontalFov = 90.0f;
	desc.nearZ = 0.1f;
	desc.isReversedZ = false;
	desc.timeScale = 1.0;
	GetCameraDescFromInputDevices(desc);

	m_Camera.Update(desc, frameIndex);
}

void Sample::RenderFrame(uint32_t frameIndex) {
	nri::Dim_t w = (nri::Dim_t)GetWindowResolution().first;
	nri::Dim_t h = (nri::Dim_t)GetWindowResolution().second;
	nri::Dim_t w2 = w / 2;
	nri::Dim_t h2 = h / 2;

	const uint32_t bufferedFrameIndex = frameIndex % BUFFERED_FRAME_MAX_NUM;
	const Frame &frame = m_Frames[bufferedFrameIndex];

	if (frameIndex >= BUFFERED_FRAME_MAX_NUM) {
		NRI.Wait(*m_FrameFence, 1 + frameIndex - BUFFERED_FRAME_MAX_NUM);
		NRI.ResetCommandAllocator(*frame.commandAllocator);
	}

	const uint32_t currentTextureIndex =
			NRI.AcquireNextSwapChainTexture(*m_SwapChain);
	BackBuffer &currentBackBuffer = m_SwapChainBuffers[currentTextureIndex];

	// Record
	nri::CommandBuffer *commandBuffer = frame.commandBuffer;
	nri::CommandBuffer *commandBufferCompute = frame.commandBufferCompute;

	nri::AttachmentsDesc presentDesc = {};

	NRI.BeginCommandBuffer(*commandBuffer, m_DescriptorPool);
	{
		nri::TextureBarrierDesc textureBarrierDescs = {};
		textureBarrierDescs.texture = currentBackBuffer.texture;
		textureBarrierDescs.after = { nri::AccessBits::COLOR_ATTACHMENT,
			nri::Layout::COLOR_ATTACHMENT };
		std::array<nri::TextureBarrierDesc, 2> texBarrierArray = { textureBarrierDescs };
		nri::BarrierGroupDesc barrierGroupDesc = {};
		barrierGroupDesc.textureNum = 1;
		barrierGroupDesc.textures = &textureBarrierDescs;

		NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);

		nri::AttachmentsDesc attachmentsDesc = {};
		attachmentsDesc.colorNum = 1;
		// attachmentsDesc.colors = &currentBackBuffer.colorAttachment;
		attachmentsDesc.colors = &m_ColorAttachment;
		attachmentsDesc.depthStencil = m_DepthAttachment;
		attachmentsDesc.viewMask = 0;

		presentDesc = attachmentsDesc;
		presentDesc.colors = &currentBackBuffer.colorAttachment;

		NRI.CmdBeginRendering(*commandBuffer, attachmentsDesc);
		{
			{
				helper::Annotation annotation(NRI, *commandBuffer, "Clears");

				nri::ClearDesc clearDesc = {};
				clearDesc.planes = nri::PlaneBits::COLOR;
				clearDesc.value.color.f = COLOR_0;

				NRI.CmdClearAttachments(*commandBuffer, &clearDesc, 1, nullptr, 0);
				clearDesc = {};
				clearDesc.planes = nri::PlaneBits::DEPTH;
				clearDesc.value.depthStencil.depth = 1.0;
				NRI.CmdClearAttachments(*commandBuffer, &clearDesc, 1, nullptr, 0);
			}
			RenderInfo info = { .desc = attachmentsDesc, .cmdBuffer = *commandBuffer };
			testRenderPtr->OnRender(info, m_Camera);
		}
		NRI.CmdEndRendering(*commandBuffer);

		// Singleview
		attachmentsDesc.viewMask = 0;

		// NRI.CmdBeginRendering(*commandBuffer, attachmentsDesc);
		// {
			
		// }
		// NRI.CmdEndRendering(*commandBuffer);

		NRI.CmdBeginRendering(*commandBuffer, presentDesc);
		{
			RenderInfo presentinfo = { .desc = presentDesc, .cmdBuffer = *commandBuffer };
			NRI.CmdSetDescriptorPool(*commandBuffer, *m_DescriptorPool);
			testRenderPtr->OnPresent(presentinfo);

			helper::Annotation annotation(NRI, *commandBuffer, "UI");
			RenderUI(NRI, NRI, *m_Streamer, *commandBuffer, 1.0f, true);
		}
		NRI.CmdEndRendering(*commandBuffer);

		textureBarrierDescs.before = textureBarrierDescs.after;
		textureBarrierDescs.after = { nri::AccessBits::UNKNOWN,
			nri::Layout::PRESENT };

		NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
	}
	NRI.EndCommandBuffer(*commandBuffer);

	nri::FenceSubmitDesc computeFinishedFence = {};
	computeFinishedFence.fence = m_ComputeFence;
	computeFinishedFence.value = 1 + frameIndex;

	// Submit Compute CommandBuffer
	{
		nri::FenceSubmitDesc waitFence = {};
		waitFence.fence = m_FrameFence;
		waitFence.value = frameIndex;

		nri::QueueSubmitDesc computeTask = {};
		computeTask.waitFences = &waitFence; // Wait for the previous frame completion before execution
		computeTask.waitFenceNum = 1;
		computeTask.commandBuffers = &frame.commandBufferCompute;
		computeTask.commandBufferNum = 1;
		computeTask.signalFences = &computeFinishedFence;
		computeTask.signalFenceNum = 1;

		NRI.QueueSubmit(*m_ComputeQueue, computeTask);
	}

	// Submit Graphics CommandBuffer
	{
		nri::FenceSubmitDesc graphicsWaitFence = {};
		graphicsWaitFence.fence = m_ComputeFence;
		graphicsWaitFence.value = frameIndex + 1;

		nri::QueueSubmitDesc queueSubmitDesc = {};
		queueSubmitDesc.waitFences = &graphicsWaitFence;
		queueSubmitDesc.waitFenceNum = 1;
		queueSubmitDesc.commandBuffers = &frame.commandBuffer;
		queueSubmitDesc.commandBufferNum = 1;

		NRI.QueueSubmit(*m_GraphicsQueue, queueSubmitDesc);
	}

	// Present
	NRI.QueuePresent(*m_SwapChain);

	{ // Signaling after "Present" improves D3D11 performance a bit
		nri::FenceSubmitDesc signalFence = {};
		signalFence.fence = m_FrameFence;
		signalFence.value = 1 + frameIndex;

		nri::QueueSubmitDesc queueSubmitDesc = {};
		queueSubmitDesc.signalFences = &signalFence;
		queueSubmitDesc.signalFenceNum = 1;

		NRI.QueueSubmit(*m_GraphicsQueue, queueSubmitDesc);
	}
}

SAMPLE_MAIN(Sample, 0);
