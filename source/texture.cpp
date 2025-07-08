#include "texture.h"
#include "renderer.h"

void Texture::Create(Renderer *renderer, nri::TextureDesc &texDesc, nri::Texture2DViewDesc &viewDesc) {
	if (texDesc.format != nri::Format::UNKNOWN) {
		m_mipNum = texDesc.mipNum;

		NRI_ABORT_ON_FAILURE(
				renderer->GetNRI().CreateTexture(*renderer->GetRenderDevice(), texDesc, m_texture));

		nri::ResourceGroupDesc resourceGroupDesc = {};
		resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
		resourceGroupDesc.textureNum = 1;
		resourceGroupDesc.textures = &m_texture;
		NRI_ABORT_ON_FAILURE(renderer->GetNRI().AllocateAndBindMemory(*renderer->GetRenderDevice(), resourceGroupDesc,
				&m_mem));
	}
	// m_views.resize(texDesc.mipNum);
	// if (viewDesc.format != nri::Format::UNKNOWN) {
	// 	viewDesc.texture = m_texture;
	// 	for (uint32_t i = 0; i < texDesc.mipNum; i++) {
	// 		viewDesc.mipOffset = i;
	// 		NRI_ABORT_ON_FAILURE(
	// 				renderer->GetNRI().CreateTexture2DView(viewDesc, m_views[i]));
	// 	}
	// }
}

void Texture::CreateAllView(Renderer *renderer, nri::Texture2DViewDesc &viewDesc) {
	m_views.resize(m_mipNum);
	if (viewDesc.format != nri::Format::UNKNOWN) {
		viewDesc.texture = m_texture;
		for (uint32_t i = 0; i < m_mipNum; i++) {
			viewDesc.mipOffset = i;
			NRI_ABORT_ON_FAILURE(
					renderer->GetNRI().CreateTexture2DView(viewDesc, m_views[i]));
		}
	}
}

void Texture::CreateView(Renderer *renderer, nri::Texture2DViewDesc &viewDesc) {
	m_views.resize(1);
	if (viewDesc.format != nri::Format::UNKNOWN) {
		viewDesc.texture = m_texture;
		NRI_ABORT_ON_FAILURE(
				renderer->GetNRI().CreateTexture2DView(viewDesc, m_views[0]));
	}
}
