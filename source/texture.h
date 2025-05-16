#pragma once
#include "NRI.h"
#include <stdint.h>
class Renderer;
class Texture {
public:
	Texture() = default;
	void Create(Renderer *renderer, nri::TextureDesc &bufferDesc, nri::Texture2DViewDesc &viewDesc);
	void Create(nri::Texture *tex, nri::Texture2DViewDesc &viewDesc);
	inline uint32_t GetViewIndex() { return m_viewIndex; }
	inline void SetViewIndex(uint32_t index) { m_viewIndex = index; }
	nri::Texture *GetTexture() { return m_texture; }
	nri::Descriptor *GetView() { return m_view; }
	bool GetUploadState() { return m_uploadState; }
	void SetUploadState(bool state) { m_uploadState = state; }

private:
	nri::Texture *m_texture;
	nri::Descriptor *m_view;
	nri::Memory *m_mem;
	uint32_t m_viewIndex = UINT32_MAX;
	bool m_uploadState = true;
};