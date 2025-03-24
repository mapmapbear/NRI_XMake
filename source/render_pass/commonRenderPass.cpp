#include "commonRenderPass.h"
#include "../renderer.h"

CommonRenderPass::CommonRenderPass(Renderer *renderer) :
		m_renderer(renderer) {
	m_NRI = &m_renderer->GetNRI();
	auto NRI = *m_NRI;
}