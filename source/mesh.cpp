#include "mesh.h"
#include "NRIDescs.h"
#include "assimp/material.h"
#include "assimp/types.h"
#include "buffer.h"
#include "renderer.h"
#include "texture.h"
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

	auto loadTexture = [renderer](std::string &basepath, aiMaterial *mat, aiTextureType type) {
		std::shared_ptr<Texture> tex = std::make_shared<Texture>();

		aiString aiStr;
		aiReturn ret = mat->GetTexture(type, 0, &aiStr);

		if (ret == aiReturn_SUCCESS) {
			std::string p = aiStr.C_Str();
			utils::Texture textureData;
			utils::LoadTexture(p, textureData);
			nri::TextureDesc textureDesc = {};
			textureDesc.type = nri::TextureType::TEXTURE_2D;
			textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
			textureDesc.format = textureData.GetFormat();
			textureDesc.width = textureData.GetWidth();
			textureDesc.height = textureData.GetHeight();
			textureDesc.mipNum = 1; //m_texureDatas[i].GetMipNum();
			textureDesc.depth = textureData.GetDepth();

			nri::Texture2DViewDesc texViewDesc = {};
			texViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D;
			texViewDesc.format = textureData.GetFormat();
			tex->Create(renderer, textureDesc, texViewDesc);

			nri::TextureUploadDesc uploadDesc = {};
			uploadDesc.texture = tex->GetTexture();
			uploadDesc.after = { nri::AccessBits::SHADER_RESOURCE,
				nri::Layout::SHADER_RESOURCE };
			uploadDesc.planes = nri::PlaneBits::ALL;
			NRI_ABORT_ON_FAILURE(renderer->GetNRI().UploadData(renderer->GetRenderQueue(), &uploadDesc, 1,
					nullptr, 0));
		} else {
			switch (type) {
				case aiTextureType_NORMAL_CAMERA:
					tex = renderer->GetDefaultNormalTexPtr();
					break;
				case aiTextureType_METALNESS:
				case aiTextureType_DIFFUSE_ROUGHNESS:
				default:
					tex = renderer->GetDefaultBlackTexPtr();
					break;
			}
		}
		return tex;
	};
	std::string dirPath = {};
	m_Materials.resize(pScene->mNumMaterials);
	for (uint32 i = 0; i < pScene->mNumMaterials; ++i) {
		Material &m = m_Materials[i];
		m.m_BaseTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_BASE_COLOR);
		m.m_NormalTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_NORMAL_CAMERA);
		m.m_MetallicTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_METALNESS);
	}
}

std::unique_ptr<SubMesh> Mesh::LoadMesh(aiMesh *pMesh, Renderer *renderer) {
	return nullptr;
}