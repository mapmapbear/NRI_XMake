// © 2021 NVIDIA Corporation
#include "GLFW/glfw3.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/random.hpp"
#include "glm/trigonometric.hpp"
#include "imgui.h"
#include "renderer.h"
#include "texture.h"

// STB
#include "spdlog/spdlog.h"
#include "stb_image.h"
#include <string>
#include <unordered_map>

#define INSTANCE

constexpr uint32_t VIEW_MASK = 0b11;
constexpr nri::Color32f COLOR_0 = { 0.0f, 0.0f, 0.0f, 0.0f };
constexpr nri::Color32f COLOR_1 = { 0.46f, 0.72f, 0.0f, 1.0f };

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
	uint32_t fenceValue = 0;
};

class Sample : public SampleBase {
public:
	Sample() {}

	~Sample();

	bool Initialize(nri::GraphicsAPI graphicsAPI) override;
	void PrepareFrame(uint32_t frameIndex) override;
	void RenderFrame(uint32_t frameIndex) override;
	utils::Scene m_Scene1;

private:
	NRIInterface NRI = {};
	nri::Device *m_Device = nullptr;
	nri::Streamer *m_Streamer = nullptr;
	nri::SwapChain *m_SwapChain = nullptr;
	nri::Queue *m_GraphicsQueue = nullptr;
	nri::Queue *m_ComputeQueue = nullptr;
	nri::Fence *m_FrameFence[BUFFERED_FRAME_MAX_NUM] = { nullptr, nullptr };
	uint64_t fenceValue[BUFFERED_FRAME_MAX_NUM] = { 0, 0 };
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
	NRI.DestroyDescriptor(*m_ColorAttachment);
	NRI.DestroyDescriptor(*m_Sampler);
	NRI.DestroyDescriptor(*m_CubeSampler);
	NRI.DestroyBuffer(*m_ConstantBuffer);
	NRI.DestroyBuffer(*m_GeometryBuffer);
	NRI.DestroyTexture(*m_Texture);
	NRI.DestroyTexture(*m_DepthTexture);
	NRI.DestroyTexture(*m_ColorTexture);
	NRI.DestroyDescriptorPool(*m_DescriptorPool);
	NRI.DestroyFence(*m_FrameFence[0]);
	NRI.DestroyFence(*m_FrameFence[1]);
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

	nri::QueueFamilyDesc queueFamilies[3] = {};
	queueFamilies[0].queueNum = 1;
	queueFamilies[0].queueType = nri::QueueType::GRAPHICS;
	queueFamilies[1].queueNum = 1;
	queueFamilies[1].queueType = nri::QueueType::COMPUTE;
	queueFamilies[2].queueNum = 1;
	queueFamilies[2].queueType = nri::QueueType::COPY;

	// Device
	nri::DeviceCreationDesc deviceCreationDesc = {};
	deviceCreationDesc.graphicsAPI = graphicsAPI;
	deviceCreationDesc.queueFamilies = queueFamilies;
	deviceCreationDesc.queueFamilyNum = helper::GetCountOf(queueFamilies);
#ifndef DEBUG
	deviceCreationDesc.enableGraphicsAPIValidation = true;
	deviceCreationDesc.enableNRIValidation = true;
#else
	deviceCreationDesc.enableGraphicsAPIValidation = false;
	deviceCreationDesc.enableNRIValidation = false;
#endif
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
	NRI_ABORT_ON_FAILURE(NRI.CreateFence(*m_Device, 0, m_FrameFence[0]));
	NRI_ABORT_ON_FAILURE(NRI.CreateFence(*m_Device, 0, m_FrameFence[1]));
	// Swap chain
	nri::Format swapChainFormat;
	{
		nri::SwapChainDesc swapChainDesc = {};
		swapChainDesc.window = GetWindow();
		swapChainDesc.queue = m_GraphicsQueue;
#ifdef HDR_ENABLE
		swapChainDesc.format = nri::SwapChainFormat::BT709_G22_10BIT;
#else
		swapChainDesc.format = nri::SwapChainFormat::BT709_G22_8BIT;
#endif
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

	utils::ShaderCodeStorage shaderCodeStorage;
	const nri::DeviceDesc &deviceDesc = NRI.GetDeviceDesc(*m_Device);

	m_DescriptorPool = &testRenderPtr->GetDescriptorPool();

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::COLOR_ATTACHMENT | nri::TextureUsageBits::SHADER_RESOURCE;
#ifdef HDR_ENABLE
		textureDesc.format = nri::Format::R10_G10_B10_A2_UNORM;
#else
		textureDesc.format = nri::Format::RGBA8_UNORM;
#endif
		textureDesc.width = (uint16_t)GetWindowResolution().first;
		textureDesc.height = (uint16_t)GetWindowResolution().second;
		textureDesc.mipNum = 1;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_Device, textureDesc, m_ColorTexture));
	}

	{
		nri::TextureDesc textureDesc = {};
		textureDesc.type = nri::TextureType::TEXTURE_2D;
		textureDesc.usage = nri::TextureUsageBits::DEPTH_STENCIL_ATTACHMENT | nri::TextureUsageBits::SHADER_RESOURCE;
		textureDesc.format = nri::Format::D32_SFLOAT;
		textureDesc.width = (uint16_t)GetWindowResolution().first;
		textureDesc.height = (uint16_t)GetWindowResolution().second;
		textureDesc.mipNum = 1;
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture(*m_Device, textureDesc, m_DepthTexture));
	}

	nri::ResourceGroupDesc resourceGroupDesc = {};
	std::vector<nri::Texture *> textureArray = { m_DepthTexture, m_ColorTexture };
	resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
	resourceGroupDesc.textureNum = (uint32_t)textureArray.size();
	resourceGroupDesc.textures = textureArray.data();

	m_MemoryAllocations.resize(
			NRI.CalculateAllocationNumber(*m_Device, resourceGroupDesc), nullptr);
	NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(
			*m_Device, resourceGroupDesc, m_MemoryAllocations.data()));

	NRI.SetDebugName(m_ColorTexture, "m_ColorTexture");
	NRI.SetDebugName(m_DepthTexture, "m_DepthTexture");

	{
		nri::Texture2DViewDesc textureViewDesc = { .texture = m_DepthTexture, .viewType = nri::Texture2DViewType::DEPTH_STENCIL_ATTACHMENT, .format = nri::Format::D32_SFLOAT };
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(textureViewDesc, m_DepthAttachment));
	}

	{
		nri::Texture2DViewDesc textureViewDesc = { .texture = m_ColorTexture, .viewType = nri::Texture2DViewType::COLOR_ATTACHMENT, .format = nri::Format::RGBA8_UNORM };
#ifdef HDR_ENABLE
		textureViewDesc.format = nri::Format::R10_G10_B10_A2_UNORM;
#endif
		NRI_ABORT_ON_FAILURE(
				NRI.CreateTexture2DView(textureViewDesc, m_ColorAttachment));
	}

	uint32_t width = GetWindowResolution().first;
	uint32_t height = GetWindowResolution().second;

	nri::TextureSubresourceUploadDesc colorSubRes = {};
	colorSubRes.rowPitch = width * 4;
	colorSubRes.slicePitch = colorSubRes.rowPitch * height;
	std::vector<uint8_t> data(colorSubRes.slicePitch, 0);
	colorSubRes.slices = data.data();
	colorSubRes.sliceNum = 1;

	nri::TextureSubresourceUploadDesc depthSubRes = {};
	depthSubRes.rowPitch = width * 4;
	depthSubRes.slicePitch = depthSubRes.rowPitch * height;
	std::vector<uint8_t> data1(colorSubRes.slicePitch, 1u);
	depthSubRes.sliceNum = 1;
	depthSubRes.slices = data1.data();

	nri::TextureUploadDesc textureData;
	textureData.subresources = &depthSubRes;
	textureData.texture = m_DepthTexture;
	// textureData.after = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT };
	textureData.after = { nri::AccessBits::COPY_DESTINATION, nri::Layout::COPY_DESTINATION };
	textureData.planes = nri::PlaneBits::ALL;

	nri::TextureUploadDesc textureData1;
	textureData1.subresources = &colorSubRes;
	textureData1.texture = m_ColorTexture;
	//textureData1.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT };
	textureData1.after = { nri::AccessBits::COPY_DESTINATION, nri::Layout::COPY_DESTINATION };
	textureData1.planes = nri::PlaneBits::ALL;

	std::vector<nri::TextureUploadDesc> texUploadDescArray = { textureData1, textureData };

	NRI_ABORT_ON_FAILURE(NRI.UploadData(*m_GraphicsQueue, texUploadDescArray.data(), (uint32_t)texUploadDescArray.size(),
			nullptr,
			0));

	std::string sceneFile = utils::GetFullPath("meshes/orrery/scene.gltf", utils::DataFolder::ROOT);
	// sceneFile = utils::GetFullPath("test.glb", utils::DataFolder::ROOT);
	NRI_ABORT_ON_FALSE(utils::LoadScene(sceneFile, m_Scene1, false));

	testRenderPtr->OnStart(nullptr, m_ColorTexture, m_DepthTexture);
	testRenderPtr->InitPresentPass(m_ColorTexture, m_SwapChain);

	// User interface
	bool initialized = InitUI(NRI, NRI, *m_Device, swapChainFormat);
	m_Camera.Initialize(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 1.0f, 0.0));
	return initialized;
}

void ShowNode(utils::NodeData node) {
	if (node.children.size() > 0) {
		if (ImGui::TreeNodeEx(node.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			for (auto &child : node.children) {
				ShowNode(child);
			}
			ImGui::TreePop();
		}
	} else {
		ImGuiTreeNodeFlags_ flag = node.isRoot ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_Leaf;
		if (ImGui::TreeNodeEx(node.name.c_str(), flag)) {
			ImGui::TreePop();
		}
	}
}

// void ShowFrameTimeGraph(size_t frameCount)
// {
//     const float width = ImGui::GetWindowWidth();
//     // const size_t frameCount = m_FrameTimeHistory.GetCount();
//     if(width > 0.f && frameCount > 0)
//     {
//         ImDrawList* drawList = ImGui::GetWindowDrawList();
//         ImVec2 basePos = ImGui::GetCursorScreenPos();
//         constexpr float minHeight = 2.f;
//         constexpr float maxHeight = 64.f;
//         float endX = width;
//         constexpr float dtMin = 1.f / 120.f;
//         constexpr float dtMax = 1.f / 15.f;
//         const float dtMin_Log2 = log2(dtMin);
//         const float dtMax_Log2 = log2(dtMax);
//         drawList->AddRectFilled(basePos, ImVec2(basePos.x + width, basePos.y + maxHeight), 0xFF404040);
//         for(size_t frameIndex = 0; frameIndex < frameCount && endX > 0.f; ++frameIndex)
//         {
//             const FrameTimeHistory::Entry dt = m_FrameTimeHistory.Get(frameIndex);
//             const float frameWidth = dt.m_DT / dtMin;
//             const float frameHeightFactor = (dt.m_DT_Log2 - dtMin_Log2) / (dtMax_Log2 - dtMin_Log2);
//             const float frameHeightFactor_Nrm = std::min(std::max(0.f, frameHeightFactor), 1.f);
//             const float frameHeight = glm::mix(minHeight, maxHeight, frameHeightFactor_Nrm);
//             const float begX = endX - frameWidth;
//             const uint32_t color = glm::packUnorm4x8(DeltaTimeToColor(dt.m_DT));
//             drawList->AddRectFilled(
//                 ImVec2(basePos.x + std::max(0.f, floor(begX)), basePos.y + maxHeight - frameHeight),
//                 ImVec2(basePos.x + ceil(endX), basePos.y + maxHeight),
//                 color);
//             endX = begX;
//         }
//         ImGui::Dummy(ImVec2(width, maxHeight));
//     }
// }

void Sample::PrepareFrame(uint32_t frameIndex) {
	BeginUI();
	ImGui::SetNextWindowPos(ImVec2(30, 30), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(0, 0));
	ImGui::Begin("Render Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	{
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Frame: %u", frameIndex);
		// ImGui::SliderFloat("Transparency", &m_Transparency, 0.0f, 1.0f);
		// ImGui::SliderFloat("Scale", &m_Scale, 0.75f, 1.25f);
		// ImGui::SliderFloat("Fov", &m_Fov, 20.0f, 120.0f, "%.0f");
		// ImGui::SliderInt("Tex Index", &testRenderPtr->testIndex, 0, 10);
		// ImGui::SliderFloat("Metallic", &testRenderPtr->testMaterial, 0.0, 1.0);
		// ImGui::SliderFloat("Roughness", &testRenderPtr->testRoughness, 0.0, 1.0);
		// ImGui::SliderFloat4("Mat Debug", &testRenderPtr->testVec.x, 0.0, 1.0);
		// ImGui::Text("Light Rotation");
		// ImGui::SliderFloat("Yaw", &testRenderPtr->testVec.x, 0.0f, 360.0f);
		// ImGui::SliderFloat("Pitch", &testRenderPtr->testVec.y, -90.0f, 90.0f);
		// ImGui::SliderFloat("Roll", &testRenderPtr->testVec.z, 0.0f, 360.0f);
		// ImGui::SliderFloat("radius", &testRenderPtr->testVec.x, 0.0f, 1.0f);
		// ImGui::SliderFloat("att", &testRenderPtr->testVec.y, 0.0f, 3.0f);
		// ImGui::SliderFloat("dist", &testRenderPtr->testVec.z, 0.0f, 3.0f);
		// ImGui::SliderFloat("radius", &testRenderPtr->testVec.x, 0.0f, 1.0f);
		// ImGui::SliderFloat4("Roll", &testRenderPtr->testVec[0], 0.0f, 360.0f);
	}
	ImGui::End();

	std::unordered_map<std::string, utils::NodeData> nodeNameMap;

	// ImGui::Begin("Scene Hierarchy");
	// {
	// 	ShowNode(m_Scene1.rootNode);
	// 	// ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	// 	// ImGui::Text("Frame: %u", frameIndex);
	// 	// ImGui::Text("Window Size: %u x %u",
	// 	// 		GetWindowResolution().first, GetWindowResolution().second);
	// 	// ImGui::Text("SwapChain Size: %u x %u",
	// 	// 		NRI.GetTextureDesc(*m_SwapChainBuffers[0].texture).width,
	// 	// 		NRI.GetTextureDesc(*m_SwapChainBuffers[0].texture).height);
	// 	// ImGui::Text("SwapChain Format: BT709_G22_8BIT");
	// }
	// ImGui::End();

	ImGui::ShowDemoWindow();

	EndUI(NRI, *m_Streamer);
	NRI.CopyStreamerUpdateRequests(*m_Streamer);

	CameraDesc desc = {};
	desc.aspectRatio = float(GetWindowResolution().first) / float(GetWindowResolution().second);
	desc.horizontalFov = glm::radians(m_Fov);
	desc.nearZ = 0.01f;
	desc.farZ = 200.0f;
	desc.isReversedZ = true;
	desc.timeScale = 5.0;
	GetCameraDescFromInputDevices(desc);
	m_Camera.Update(desc, frameIndex);

	float deltaTime = (float)glfwGetTime();
	testRenderPtr->OnUpdate(deltaTime);
}

void Sample::RenderFrame(uint32_t frameIndex) {
	nri::Dim_t w = (nri::Dim_t)GetWindowResolution().first;
	nri::Dim_t h = (nri::Dim_t)GetWindowResolution().second;
	nri::Dim_t w2 = w / 2;
	nri::Dim_t h2 = h / 2;

	const uint32_t bufferedFrameIndex = frameIndex % BUFFERED_FRAME_MAX_NUM;

	Frame &frame = m_Frames[bufferedFrameIndex];

	const uint32_t currentTextureIndex =
			NRI.AcquireNextSwapChainTexture(*m_SwapChain);
	BackBuffer &currentBackBuffer = m_SwapChainBuffers[currentTextureIndex];

	// Record
	nri::CommandBuffer *commandBuffer = frame.commandBuffer;

	nri::AttachmentsDesc presentDesc = {};

	NRI.BeginCommandBuffer(*commandBuffer, m_DescriptorPool);
	{
		// Transform Back Buffer
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = currentBackBuffer.texture;
			textureBarrierDescs.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT };

			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;

			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}

		// Transform Color RT
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = m_ColorTexture;
			if (frameIndex == 0) {
				textureBarrierDescs.before = { nri::AccessBits::COPY_DESTINATION, nri::Layout::COPY_DESTINATION };
			}
			textureBarrierDescs.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT };

			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;

			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}

		// Transform Depth RT
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = m_DepthTexture;
			if (frameIndex == 0) {
				textureBarrierDescs.before = { nri::AccessBits::COPY_DESTINATION, nri::Layout::COPY_DESTINATION };
			} else {
				textureBarrierDescs.before = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT };
			}
			textureBarrierDescs.after = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT };

			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;

			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}

		nri::AttachmentsDesc attachmentsDesc = {};
		attachmentsDesc.colorNum = 1;
		attachmentsDesc.colors = &m_ColorAttachment;
		attachmentsDesc.depthStencil = m_DepthAttachment;
		attachmentsDesc.viewMask = 0;

		presentDesc = attachmentsDesc;
		presentDesc.colors = &currentBackBuffer.colorAttachment;
		presentDesc.depthStencil = nullptr;


		nri::AttachmentsDesc depthAttachmentsDesc = attachmentsDesc;
		depthAttachmentsDesc.colorNum = 0;
		depthAttachmentsDesc.colors = nullptr;

		// NRI.CmdBeginRendering(*commandBuffer, depthAttachmentsDesc);
		// {
		// 	RenderInfo info = { .desc = attachmentsDesc, .cmdBuffer = *commandBuffer };
		// 	testRenderPtr->OnRender(info, m_Camera);
		// }
		// NRI.CmdEndRendering(*commandBuffer);

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
				clearDesc.value.depthStencil.depth = 0.0;
				NRI.CmdClearAttachments(*commandBuffer, &clearDesc, 1, nullptr, 0);
			}
		}
		NRI.CmdEndRendering(*commandBuffer);

		RenderInfo info = { .desc = attachmentsDesc, .cmdBuffer = *commandBuffer };
		testRenderPtr->OnRender(info, m_Camera);

		// Transform Depth RT -> Depth SRV
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = m_DepthTexture;
			textureBarrierDescs.before = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE,
				nri::Layout::DEPTH_STENCIL_ATTACHMENT };
			textureBarrierDescs.after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;
			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}

		testRenderPtr->OnRenderDepth(info, m_Camera);
		// Transform Color RT -> Back Buffer
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = m_ColorTexture;
			textureBarrierDescs.before = { nri::AccessBits::COLOR_ATTACHMENT,
				nri::Layout::COLOR_ATTACHMENT };
			textureBarrierDescs.after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;
			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}

		NRI.CmdBeginRendering(*commandBuffer, presentDesc);
		{
			RenderInfo presentinfo = { .desc = presentDesc, .cmdBuffer = *commandBuffer };
			NRI.CmdSetDescriptorPool(*commandBuffer, *m_DescriptorPool);
			testRenderPtr->OnPresent(presentinfo);
			helper::Annotation annotation(NRI, *commandBuffer, "UI");
			RenderUI(NRI, NRI, *m_Streamer, *commandBuffer, 1.0f, true);
		}
		NRI.CmdEndRendering(*commandBuffer);

		// Transform Color RT -> Next Frame
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = m_ColorTexture;
			textureBarrierDescs.before = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			textureBarrierDescs.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT };
			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;
			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}

		// Transform Depth RT -> Next Frame
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = m_DepthTexture;
			textureBarrierDescs.before = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			textureBarrierDescs.after = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE,
				nri::Layout::DEPTH_STENCIL_ATTACHMENT };

			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;
			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}

		// Transform ShadowMap -> Next Frame
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = testRenderPtr->m_ShadowMap->GetTexture();
			// textureBarrierDescs.before = textureBarrierDescs.after;
			textureBarrierDescs.after = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE,
				nri::Layout::DEPTH_STENCIL_ATTACHMENT };

			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;
			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}


		// Transform Back Buffer -> Next Frame
		{
			nri::TextureBarrierDesc textureBarrierDescs = {};
			textureBarrierDescs.texture = currentBackBuffer.texture;
			textureBarrierDescs.before = textureBarrierDescs.after;
			textureBarrierDescs.after = { nri::AccessBits::UNKNOWN,
				nri::Layout::PRESENT };

			nri::BarrierGroupDesc barrierGroupDesc = {};
			barrierGroupDesc.textureNum = 1;
			barrierGroupDesc.textures = &textureBarrierDescs;
			NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
		}
	}
	NRI.EndCommandBuffer(*commandBuffer);

	{ // Submit
		nri::QueueSubmitDesc queueSubmitDesc = {};
		queueSubmitDesc.commandBuffers = &frame.commandBuffer;
		queueSubmitDesc.commandBufferNum = 1;

		NRI.QueueSubmit(*m_GraphicsQueue, queueSubmitDesc);
	}

	{
		nri::FenceSubmitDesc signalFence = {};
		signalFence.fence = m_FrameFence[bufferedFrameIndex];
		signalFence.value = ++frame.fenceValue;

		nri::QueueSubmitDesc queueSubmitDesc = {};
		queueSubmitDesc.signalFences = &signalFence;
		queueSubmitDesc.signalFenceNum = 1;

		NRI.QueueSubmit(*m_GraphicsQueue, queueSubmitDesc);
	}

	NRI.QueuePresent(*m_SwapChain);

	if (frameIndex >= BUFFERED_FRAME_MAX_NUM) {
		NRI.Wait(*m_FrameFence[bufferedFrameIndex], frame.fenceValue);
		NRI.ResetCommandAllocator(*frame.commandAllocator);
	}
}

SAMPLE_MAIN(Sample, 0);
