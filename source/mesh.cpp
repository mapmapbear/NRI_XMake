#include "mesh.h"
#include "assimp/material.h"
#include "assimp/types.h"
#include "renderer.h"
#include "texture.h"
#include "buffer.h"
#include <assimp/Importer.hpp>
#include <memory>

void Mesh::LoadFromUSD(std::string &path, Renderer *renderer) {
	struct Vertex {
		glm::vec3 Position;
		glm::vec2 TexCoord;
		glm::vec3 Normal;
	};

	Assimp::Importer importer;
	const aiScene *pScene = importer.ReadFile(path,
			aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace | aiProcess_GenUVCoords);

	for (uint32 i = 0; i < pScene->mNumMeshes; ++i) {
		m_Meshes.push_back(LoadMesh(pScene->mMeshes[i], renderer));
	}

	auto LoadTexture = [renderer](std::string &basepath, aiMaterial *mat, aiTextureType type) {
		std::unique_ptr<Texture> tex = std::make_unique<Texture>();

		aiString aiStr;
		aiReturn ret = mat->GetTexture(type, 0, &aiStr);

		if (ret == aiReturn_SUCCESS) {
			std::string p = aiStr.C_Str();
			std::stringstream str;
			str << basepath << p;
			// tex->Create(renderer, str.str());
		} else {
			switch (type) {
				case aiTextureType_NORMALS:
					// pTex->Create(pGraphics, pContext, "Resources/textures/dummy_ddn.png", TextureUsage::ShaderResource);
					break;
				case aiTextureType_SPECULAR:
					// pTex->Create(pGraphics, pContext, "Resources/textures/dummy_specular.png", TextureUsage::ShaderResource);
					break;
				case aiTextureType_DIFFUSE:
				default:
					// pTex->Create(pGraphics, pContext, "Resources/textures/dummy.png", TextureUsage::ShaderResource);
					break;
			}
		}
	};
}