#include "mesh.h"
#include "NRIDescs.h"
#include "assimp/GltfMaterial.h"
#include "assimp/material.h"
#include "assimp/postprocess.h"
#include "assimp/types.h"
#include "buffer.h"
#include "glm/gtc/type_ptr.hpp"
#include "renderer.h"
#include "texture.h"
#include <meshoptimizer.h>
#include <assimp/Importer.hpp>
#include <map>
#include <memory>
#include <set>
#include <utility>
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
			aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace | aiProcess_GenUVCoords | aiProcess_GenBoundingBoxes);

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

	m_Materials.resize(pScene->mNumMaterials);
	for (uint32 i = 0; i < pScene->mNumMaterials; ++i) {
		Material &m = m_Materials[i];
		m.m_BaseTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_BASE_COLOR);
		m.m_NormalTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_NORMAL_CAMERA);
		m.m_MetallicTexture = loadTexture(dirPath, pScene->mMaterials[i], aiTextureType_METALNESS);
		aiString alphaMode;
		if (pScene->mMaterials[i]->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
			if (alphaMode == aiString("BLEND")) {
				SPDLOG_INFO("Material is translucent (AlphaMode: BLEND)");
				m.IsTransparent = true;
			} else if (alphaMode == aiString("MASK")) {
				float alphaCutoff = 0.5f;
				pScene->mMaterials[i]->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);
				SPDLOG_INFO("Material uses Alpha Test (AlphaMode: MASK, Cutoff: {}", alphaCutoff);
				m.IsTransparent = true;
			} else if (alphaMode == aiString("OPAQUE")) {
				// std::cout << "Material is opaque (AlphaMode: OPAQUE)" << std::endl;
				m.IsTransparent = false;
			}
		} else {
			std::cout << "No AlphaMode specified, assuming opaque" << std::endl;
			m.IsTransparent = false;
		}
	}
}

std::unique_ptr<SubMesh> Mesh::LoadMesh(aiMesh *pMesh, Renderer *renderer) {
	std::shared_ptr<utils::MeshData> meshdata = std::make_shared<utils::MeshData>();
	meshdata->m_vertexesData.resize(pMesh->mNumVertices);
	meshdata->vertices.resize(pMesh->mNumVertices);
	meshdata->indices.resize(pMesh->mNumFaces * 3);
	const aiAABB &aabb = pMesh->mAABB;
	for (uint32 j = 0; j < pMesh->mNumVertices; ++j) {
		utils::Vertex &vertex = meshdata->m_vertexesData[j];
		vertex.position = *reinterpret_cast<glm::vec3 *>(&pMesh->mVertices[j]);
		meshdata->vertices[j] = *reinterpret_cast<glm::vec3 *>(&pMesh->mVertices[j]);
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

	size_t vertexCount = meshdata->m_vertexesData.size();
	size_t indexCount = meshdata->indices.size();

	std::vector<unsigned int> remap(vertexCount);
	size_t newVertexCount = meshopt_generateVertexRemap(remap.data(), meshdata->indices.data(), indexCount, meshdata->m_vertexesData.data(), vertexCount, sizeof(utils::Vertex));

	std::vector<utils::Vertex> remappedVertices(newVertexCount);
	std::vector<uint32_t> remappedIndices(indexCount);
	meshopt_remapVertexBuffer(remappedVertices.data(), meshdata->m_vertexesData.data(), vertexCount, sizeof(utils::Vertex), remap.data());
	meshopt_remapIndexBuffer(remappedIndices.data(), meshdata->indices.data(), indexCount, remap.data());

	std::vector<uint32_t> optimizedIndices(indexCount);
	meshopt_optimizeVertexCache(optimizedIndices.data(), remappedIndices.data(), indexCount, newVertexCount);

	std::vector<uint32_t> shadow_indices(indexCount);
	meshopt_generateShadowIndexBuffer(shadow_indices.data(), optimizedIndices.data(), indexCount, remappedVertices.data(), newVertexCount, sizeof(glm::vec3), sizeof(utils::Vertex));

	meshdata->indices = remappedIndices;
	meshdata->shadow_indices = shadow_indices;
	meshdata->m_vertexesData = remappedVertices;

	std::unique_ptr<SubMesh> pSubMesh = std::make_unique<SubMesh>();
	{
		pSubMesh->aabb = std::make_pair(glm::make_vec3((float*)&aabb.mMin.x), glm::make_vec3((float*)&aabb.mMax));
		pSubMesh->m_indexCount = (int)meshdata->indices.size();
		pSubMesh->m_indexbuffer = std::make_unique<Buffer>();
		pSubMesh->m_vertexbuffer = std::make_unique<Buffer>();
		uint32_t indicesAlignSize = static_cast<uint32_t>(helper::Align(helper::GetByteSizeOf(meshdata->indices), 32));
		uint32_t shadow_indicesAlignSize = static_cast<uint32_t>(helper::Align(helper::GetByteSizeOf(meshdata->shadow_indices), 32));
		uint32_t vertexSize = static_cast<uint32_t>(helper::GetByteSizeOf(meshdata->m_vertexesData));
		nri::BufferDesc desc = {};
		desc.size = indicesAlignSize + vertexSize;
		pSubMesh->vertexOffset = indicesAlignSize;
		desc.usage = nri::BufferUsageBits::VERTEX_BUFFER |
				nri::BufferUsageBits::INDEX_BUFFER;
		nri::BufferViewDesc viewDesc{};

		pSubMesh->m_indexbuffer->Create(renderer, desc, viewDesc);
		desc.size = shadow_indicesAlignSize + static_cast<uint32_t>(helper::GetByteSizeOf(meshdata->vertices));
		pSubMesh->m_vertexbuffer->Create(renderer, desc, viewDesc);
		renderer->uploadIndexBufferMap.insert({ pSubMesh->m_indexbuffer, meshdata });
		renderer->uploadShadowIndexBufferMap.insert({ pSubMesh->m_vertexbuffer, meshdata });
	}
	pSubMesh->m_materialID = pMesh->mMaterialIndex;

	return pSubMesh;
}
