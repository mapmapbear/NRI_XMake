#include "mesh.h"
#include "NRIDescs.h"
#include "assimp/material.h"
#include "assimp/types.h"
#include "buffer.h"
#include "renderer.h"
#include "texture.h"
#include <assimp/Importer.hpp>
#include <map>
#include <memory>
#include <vector>

void TraverseNodes(const aiScene *scene, const aiNode *node, const glm::mat4 &parentTransform, std::map<uint32_t, glm::mat4> &results) {
	aiMatrix4x4 nodeTransform = node->mTransformation;
	glm::mat4 glmNodeTransform = glm::mat4(
			nodeTransform.a1, nodeTransform.b1, nodeTransform.c1, nodeTransform.d1,
			nodeTransform.a2, nodeTransform.b2, nodeTransform.c2, nodeTransform.d2,
			nodeTransform.a3, nodeTransform.b3, nodeTransform.c3, nodeTransform.d3,
			nodeTransform.a4, nodeTransform.b4, nodeTransform.c4, nodeTransform.d4);
	glm::mat4 globalTransform = parentTransform * glmNodeTransform; // 全局变换 = 父节点变换 * 当前节点局部变换
	for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
		unsigned int meshID = node->mMeshes[i]; // 获取 meshID
		results.insert({ meshID, globalTransform }); // 存储结果
	}

	for (unsigned int i = 0; i < node->mNumChildren; ++i) {
		TraverseNodes(scene, node->mChildren[i], globalTransform, results);
	}
}

void Mesh::LoadFromUSD(std::string &path, Renderer *renderer) {
	struct Vertex {
		glm::vec3 Position;
		glm::vec2 TexCoord;
		glm::vec3 Normal;
	};

	Assimp::Importer importer;
	const aiScene *pScene = importer.ReadFile(path,
			aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace | aiProcess_GenUVCoords);

	glm::mat4 identity = glm::mat4(1.0f);
	TraverseNodes(pScene, pScene->mRootNode, identity, results);

	for (uint32 i = 0; i < pScene->mNumMeshes; ++i) {
		m_Meshes.push_back(LoadMesh(pScene->mMeshes[i], renderer));
	}

	auto loadTexture = [renderer](std::string &basepath, aiMaterial *mat, aiTextureType type) {
		std::shared_ptr<Texture> tex = std::make_shared<Texture>();

		aiString aiStr;
		aiReturn ret = mat->GetTexture(type, 0, &aiStr);

		if (ret == aiReturn_SUCCESS) {
			std::string p = aiStr.C_Str();
			p = basepath + '/' + p;
			std::shared_ptr<utils::Texture> textureData = std::make_shared<utils::Texture>();
			utils::LoadTexture(p, *textureData.get(), false);
			nri::TextureDesc textureDesc = {};
			textureDesc.type = nri::TextureType::TEXTURE_2D;
			textureDesc.usage = nri::TextureUsageBits::SHADER_RESOURCE;
			textureDesc.format = textureData->GetFormat();
			textureDesc.width = textureData->GetWidth();
			textureDesc.height = textureData->GetHeight();
			textureDesc.mipNum = 1; //m_texureDatas[i].GetMipNum();
			textureDesc.depth = textureData->GetDepth();

			nri::Texture2DViewDesc texViewDesc = {};
			texViewDesc.viewType = nri::Texture2DViewType::SHADER_RESOURCE_2D;
			texViewDesc.format = textureData->GetFormat();
			tex->Create(renderer, textureDesc, texViewDesc);
			renderer->uploadTextureMap.insert({ tex, textureData });
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
	std::string dirPath = path.substr(0, path.find_last_of("/\\"));
	;
	m_Materials.resize(pScene->mNumMaterials);
	for (uint32 i = 0; i < pScene->mNumMaterials; ++i) {
		Material &m = m_Materials[i];
		m.m_BaseTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_BASE_COLOR);
		m.m_NormalTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_NORMAL_CAMERA);
		m.m_MetallicTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_METALNESS);
	}
}

std::unique_ptr<SubMesh> Mesh::LoadMesh(aiMesh *pMesh, Renderer *renderer) {
	std::shared_ptr<utils::MeshData> meshdata = std::make_shared<utils::MeshData>();
	meshdata->m_vertexesData.resize(pMesh->mNumVertices);
	meshdata->indices.resize(pMesh->mNumFaces * 3);

	for (uint32 j = 0; j < pMesh->mNumVertices; ++j) {
		utils::Vertex &vertex = meshdata->m_vertexesData[j];
		vertex.position = *reinterpret_cast<glm::vec3 *>(&pMesh->mVertices[j]);
		if (pMesh->HasTextureCoords(0)) {
			vertex.uv = *reinterpret_cast<glm::vec2 *>(&pMesh->mTextureCoords[0][j]);
		}
		vertex.normal = *reinterpret_cast<glm::vec3 *>(&pMesh->mNormals[j]);
		if (pMesh->HasTangentsAndBitangents()) {
			vertex.tangent = *reinterpret_cast<glm::vec3 *>(&pMesh->mTangents[j]);
			vertex.bitangent = *reinterpret_cast<glm::vec3 *>(&pMesh->mBitangents[j]);
		}
	}

	for (uint32 j = 0; j < pMesh->mNumFaces; ++j) {
		const aiFace &face = pMesh->mFaces[j];
		for (uint32 k = 0; k < 3; ++k) {
			assert(face.mNumIndices == 3);
			meshdata->indices[j * 3 + k] = face.mIndices[k];
		}
	}

	std::unique_ptr<SubMesh> pSubMesh = std::make_unique<SubMesh>();
	{
		pSubMesh->m_indexCount = (int)meshdata->indices.size();
		pSubMesh->m_indexbuffer = std::make_unique<Buffer>();
		uint32_t indicesAlignSize = helper::Align(helper::GetByteSizeOf(meshdata->indices), 32);
		uint32_t vertexSize = helper::GetByteSizeOf(meshdata->m_vertexesData);
		nri::BufferDesc desc = {};
		desc.size = indicesAlignSize + vertexSize;
		pSubMesh->vertexOffset = desc.size;
		desc.usage = nri::BufferUsageBits::VERTEX_BUFFER |
				nri::BufferUsageBits::INDEX_BUFFER;
		nri::BufferViewDesc viewDesc{};

		pSubMesh->m_indexbuffer->Create(renderer, desc, viewDesc);
		// pSubMesh->m_vertexbuffer = pSubMesh->m_indexbuffer;
		renderer->uploadBufferMap.insert({ pSubMesh->m_indexbuffer, meshdata });
	}
	pSubMesh->m_materialID = pMesh->mMaterialIndex;
	return pSubMesh;
}