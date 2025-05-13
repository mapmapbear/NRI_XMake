#pragma once
#include "NRI.h"
#include <stdint.h>
class Renderer;
class Texture {
public:
	Texture() = default;
	Texture(nri::Texture *tex) :
			m_texture(tex){};
	void Create(Renderer *renderer, nri::TextureDesc *bufferDesc, nri::Texture2DViewDesc *viewDesc);
	uint32_t GetViewIndex() { return m_viewIndex; }

private:
	nri::Texture *m_texture;
	nri::Descriptor *m_view;
	nri::Memory *m_mem;
	uint32_t m_viewIndex = UINT32_MAX;
};