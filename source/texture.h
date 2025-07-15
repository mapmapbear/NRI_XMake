#pragma once
#include "NRI.h"
#include <stdint.h>
#include <vector>
class Renderer;
class Texture {
public:
	Texture() = default;
	void Create(Renderer *renderer, nri::TextureDesc &bufferDesc, nri::Texture2DViewDesc &viewDesc);
	// void Create(nri::Texture *tex, nri::Texture2DViewDesc &viewDesc);
	void CreateAllView(Renderer *renderer, nri::Texture2DViewDesc &viewDesc);
	void CreateView(Renderer *renderer, nri::Texture2DViewDesc &viewDesc, uint32_t index = 0);
	inline uint32_t GetViewIndex() { return m_viewIndex; }
	inline void SetViewIndex(uint32_t index) { m_viewIndex = index; }
	nri::Texture *GetTexture() { return m_texture; }
	nri::Descriptor *GetView(uint32_t index = 0) { return m_views[index]; }
	bool GetUploadState() { return m_uploadState; }
	void SetUploadState(bool state) { m_uploadState = state; }
	inline uint32_t GetMipNum() { return m_mipNum; }
	bool m_isDefault = false;

private:
	nri::Texture *m_texture;
	std::vector<nri::Descriptor *> m_views;
	nri::Memory *m_mem;
	uint32_t m_viewIndex = UINT32_MAX;
	bool m_uploadState = true;
	uint32_t m_mipNum = 0;
};