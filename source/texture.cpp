#include "texture.h"
#include "renderer.h"

void Texture::Create(Renderer *renderer, nri::TextureDesc &texDesc, nri::Texture2DViewDesc &viewDesc) {
	if (texDesc.format != nri::Format::UNKNOWN) {
		NRI_ABORT_ON_FAILURE(
				renderer->GetNRI().CreateTexture(*renderer->GetRenderDevice(), texDesc, m_texture));

		nri::ResourceGroupDesc resourceGroupDesc = {};
		resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
		resourceGroupDesc.textureNum = 1;
		resourceGroupDesc.textures = &m_texture;
		NRI_ABORT_ON_FAILURE(renderer->GetNRI().AllocateAndBindMemory(*renderer->GetRenderDevice(), resourceGroupDesc,
				&m_mem));
	}

	if (viewDesc.format != nri::Format::UNKNOWN) {
		viewDesc.texture = m_texture;
		NRI_ABORT_ON_FAILURE(
				renderer->GetNRI().CreateTexture2DView(viewDesc, m_view));
	}
}