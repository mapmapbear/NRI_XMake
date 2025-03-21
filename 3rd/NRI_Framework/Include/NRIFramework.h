// © 2021 NVIDIA Corporation

#pragma once

#include <stdint.h>
#define NRI_FRAMEWORK_VERSION_MAJOR 0
#define NRI_FRAMEWORK_VERSION_MINOR 15
#define NRI_FRAMEWORK_VERSION_DATE "5 February 2025"
#define NRI_FRAMEWORK 1

#include <array>
#include <string>
#include <vector>

// 3rd party
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include "GLFW/glfw3.h"
#include "imgui.h"

// NRI: core & common extensions
#include "NRI.h"

#include "Extensions/NRIDeviceCreation.h"
#include "Extensions/NRIHelper.h"
#include "Extensions/NRILowLatency.h"
#include "Extensions/NRIMeshShader.h"
#include "Extensions/NRIRayTracing.h"
#include "Extensions/NRIResourceAllocator.h"
#include "Extensions/NRIStreamer.h"
#include "Extensions/NRISwapChain.h"
#include "Extensions/NRIUpscaler.h"

// 3rd party
#include "CmdLine.h" // https://github.com/tanakh/cmdline

// #include "ml.h"
// #include "ml.hlsli"

// NRI framework
#include "Camera.h"
#include "Controls.h"
#include "Helper.h"
#include "Timer.h"
#include "Utils.h"

// Settings
constexpr nri::VKBindingOffsets VK_BINDING_OFFSETS = { 100, 200, 300, 400 }; // just ShaderMake defaults for simplicity
constexpr bool D3D11_COMMANDBUFFER_EMULATION = false;
constexpr uint32_t DEFAULT_MEMORY_ALIGNMENT = 16;
constexpr uint32_t BUFFERED_FRAME_MAX_NUM = 2;
constexpr uint32_t SWAP_CHAIN_TEXTURE_NUM = 2;

struct NRIInterface : public nri::CoreInterface,
					  public nri::HelperInterface,
					  public nri::StreamerInterface,
					  public nri::SwapChainInterface {};

struct BackBuffer {
	nri::Descriptor *colorAttachment;
	// nri::Descriptor* depthAttachment;
	nri::Texture *texture;
	// nri::Texture* depthTexture;
};

class SampleBase {
public:
	// Pre initialize
	SampleBase();

	virtual void InitCmdLine([[maybe_unused]] cmdline::parser &cmdLine) {
	}

	virtual void ReadCmdLine([[maybe_unused]] cmdline::parser &cmdLine) {
	}

	// Initialize
	virtual bool Initialize(nri::GraphicsAPI graphicsAPI) = 0;
	bool InitUI(const nri::CoreInterface &NRI, const nri::HelperInterface &helperInterface, nri::Device &device, nri::Format renderTargetFormat);

	// Wait before input (wait for latency and/or queued frames)
	virtual void LatencySleep(uint32_t frameIndex) {
		(void)frameIndex;
	}

	// Prepare
	virtual void PrepareFrame(uint32_t frameIndex) = 0;
	void BeginUI();
	//      Imgui::
	void EndUI(const nri::StreamerInterface &streamerInterface, nri::Streamer &streamer);

	// Render
	virtual void RenderFrame(uint32_t frameIndex) = 0;
	void RenderUI(const nri::CoreInterface &NRI, const nri::StreamerInterface &streamerInterface, nri::Streamer &streamer, nri::CommandBuffer &commandBuffer, float sdrScale, bool isSrgb);

	// Destroy
	virtual ~SampleBase();
	void DestroyUI(const nri::CoreInterface &NRI);

	// Misc
	virtual bool AppShouldClose() {
		return false;
	}

	inline bool IsKeyToggled(Key key) {
		bool state = m_KeyToggled[(uint32_t)key];
		m_KeyToggled[(uint32_t)key] = false;

		return state;
	}

	inline bool IsKeyPressed(Key key) const {
		return m_KeyState[(uint32_t)key];
	}

	inline bool IsButtonPressed(Button button) const {
		return m_ButtonState[(uint8_t)button];
	}

	inline const vec2 &GetMouseDelta() const {
		return m_MouseDelta;
	}

	inline float GetMouseWheel() const {
		return m_MouseWheel;
	}

	inline std::pair<uint32_t, uint32_t> GetWindowResolution() const {
		return m_WindowResolution;
	}

	inline std::pair<uint32_t, uint32_t> GetOutputResolution() const {
		return m_OutputResolution;
	}

	inline const nri::Window &GetWindow() const {
		return m_NRIWindow;
	}

	void GetCameraDescFromInputDevices(CameraDesc &cameraDesc);

	static void EnableMemoryLeakDetection(uint32_t breakOnAllocationIndex);

protected:
	nri::AllocationCallbacks m_AllocationCallbacks = {};
	std::string m_SceneFile = "ShaderBalls/ShaderBalls.gltf";
	GLFWwindow *m_Window = nullptr;
	Camera m_Camera;
	Timer m_Timer;
	std::pair<uint32_t, uint32_t> m_OutputResolution = { 900, 600 };
	std::pair<uint32_t, uint32_t> m_WindowResolution = {};
	uint8_t m_VsyncInterval = 0;
	uint32_t m_DpiMode = 0;
	uint32_t m_RngState = 0;
	float m_MouseSensitivity = 1.0f;
	bool m_DebugAPI = false;
	bool m_DebugNRI = false;
	bool m_IsActive = true;

	// Private
private:
	void CursorMode(int32_t mode);

public:
	inline bool HasUserInterface() const {
		return m_TimePrev != 0.0;
	}

	void InitCmdLineDefault(cmdline::parser &cmdLine);
	void ReadCmdLineDefault(cmdline::parser &cmdLine);
	bool Create(int32_t argc, char **argv, const char *windowTitle);
	void RenderLoop();

	// Input (not public)
public:
	std::array<bool, (size_t)Key::NUM> m_KeyState = {};
	std::array<bool, (size_t)Key::NUM> m_KeyToggled = {};
	std::array<bool, (size_t)Button::NUM> m_ButtonState = {};
	std::array<bool, (size_t)Button::NUM> m_ButtonJustPressed = {};
	vec2 m_MouseDelta = {};
	vec2 m_MousePosPrev = {};
	float m_MouseWheel = 0.0f;

private:
	// UI
	std::vector<uint8_t> m_UiData;
	nri::DescriptorPool *m_DescriptorPool = nullptr;
	nri::DescriptorSet *m_DescriptorSet = nullptr;
	nri::Descriptor *m_FontShaderResource = nullptr;
	nri::Descriptor *m_Sampler = nullptr;
	nri::Pipeline *m_Pipeline = nullptr;
	nri::PipelineLayout *m_PipelineLayout = nullptr;
	nri::Texture *m_FontTexture = nullptr;
	nri::Memory *m_FontTextureMemory = nullptr;
	GLFWcursor *m_MouseCursors[ImGuiMouseCursor_COUNT] = {};
	double m_TimePrev = 0.0;
	uint64_t m_IbOffset = 0;
	uint64_t m_VbOffset = 0;

	nri::Window m_NRIWindow = {};

	// Rendering
	uint32_t m_FrameNum = uint32_t(-1);
	uint32_t m_StreamBufferSize = 0;
};

#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

#define SAMPLE_MAIN(className, memoryAllocationIndexForBreak)                 \
	int main(int argc, char **argv) {                                         \
		SampleBase::EnableMemoryLeakDetection(memoryAllocationIndexForBreak); \
		SampleBase *sample = new className;                                   \
		bool result = sample->Create(argc, argv, STRINGIFY(PROJECT_NAME));    \
		if (result)                                                           \
			sample->RenderLoop();                                             \
		delete sample;                                                        \
		return result ? 0 : 1;                                                \
	}
