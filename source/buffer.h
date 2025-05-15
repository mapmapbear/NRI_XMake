#pragma once
#include "NRI.h"
#include <stdint.h>
class Renderer;
class Buffer {
public:
	Buffer() = default;
	void Create(Renderer *renderer, nri::BufferDesc &bufferDesc, nri::BufferViewDesc &viewDesc);
	uint32_t GetViewIndex() { return m_viewIndex; }
	nri::Buffer *GetBuffer() { return m_buffer; }

private:
	nri::Buffer *m_buffer;
	nri::Descriptor *m_view;
	nri::Memory *m_mem;
	uint32_t m_viewIndex = UINT32_MAX;
};