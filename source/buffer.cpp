#include "buffer.h"
#include "renderer.h"

void Buffer::Create(Renderer *renderer, nri::BufferDesc &bufferDesc, nri::BufferViewDesc &viewDesc) {
	if (bufferDesc.size > 0) {
		NRI_ABORT_ON_FAILURE(
				renderer->GetNRI().CreateBuffer(*renderer->GetRenderDevice(), bufferDesc, m_buffer));

		nri::ResourceGroupDesc resourceGroupDesc = {};
		resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
		resourceGroupDesc.bufferNum = 1;
		resourceGroupDesc.buffers = &m_buffer;
		NRI_ABORT_ON_FAILURE(renderer->GetNRI().AllocateAndBindMemory(*renderer->GetRenderDevice(), resourceGroupDesc,
				&m_mem));
	}

	if (viewDesc.size > 0) {
		viewDesc.buffer = m_buffer;
		NRI_ABORT_ON_FAILURE(
				renderer->GetNRI().CreateBufferView(viewDesc, m_view));
	}
}