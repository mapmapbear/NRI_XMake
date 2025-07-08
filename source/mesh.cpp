#include "mesh.h"
#include "NRIDescs.h"
#include "assimp/GltfMaterial.h"
#include "assimp/material.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/types.h"
#include "buffer.h"
#include "glm/gtc/type_ptr.hpp"
#include "renderer.h"
#include "spdlog/spdlog.h"
#include "texture.h"
#include <stdint.h>
#include <string.h>
#include <assimp/Importer.hpp>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

void TraverseNodes(const aiScene *scene, const aiNode *node, const glm::mat4 &parentTransform, std::map<uint32_t, glm::mat4> &results) {
	aiMatrix4x4 nodeTransform = node->mTransformation;
	glm::mat4 glmNodeTransform = glm::mat4(
			nodeTransform.a1, nodeTransform.b1, nodeTransform.c1, nodeTransform.d1,
			nodeTransform.a2, nodeTransform.b2, nodeTransform.c2, nodeTransform.d2,
			nodeTransform.a3, nodeTransform.b3, nodeTransform.c3, nodeTransform.d3,
			nodeTransform.a4, nodeTransform.b4, nodeTransform.c4, nodeTransform.d4);
	glm::mat4 globalTransform = parentTransform * glmNodeTransform;
	for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
		unsigned int meshID = node->mMeshes[i];
		results.insert({ meshID, globalTransform });
	}

	for (unsigned int i = 0; i < node->mNumChildren; ++i) {
		TraverseNodes(scene, node->mChildren[i], globalTransform, results);
	}
}
void TraverseNodesWithMesh(const aiScene *scene, const aiNode *node, const glm::mat4 &parentTransform, std::unordered_map<uint32_t, glm::mat4> &meshTransforms) {
	// 将aiMatrix4x4转换为glm::mat4
	aiMatrix4x4 nodeTransform = node->mTransformation;
	glm::mat4 glmNodeTransform = glm::mat4(
			nodeTransform.a1, nodeTransform.b1, nodeTransform.c1, nodeTransform.d1,
			nodeTransform.a2, nodeTransform.b2, nodeTransform.c2, nodeTransform.d2,
			nodeTransform.a3, nodeTransform.b3, nodeTransform.c3, nodeTransform.d3,
			nodeTransform.a4, nodeTransform.b4, nodeTransform.c4, nodeTransform.d4);

	// 计算当前节点的全局变换
	glm::mat4 globalTransform = parentTransform * glmNodeTransform;

	// 如果当前节点包含mesh，存储mesh ID和对应的全局变换
	for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
		unsigned int meshID = node->mMeshes[i];
		meshTransforms.insert({ meshID, globalTransform });
	}

	// 递归处理所有子节点
	for (unsigned int i = 0; i < node->mNumChildren; ++i) {
		TraverseNodesWithMesh(scene, node->mChildren[i], globalTransform, meshTransforms);
	}
}

void Mesh::LoadFromUSD(std::string &path, Renderer *renderer, bool meshlet) {
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

	std::unordered_map<uint32_t, glm::mat4> meshTransforms;
	TraverseNodesWithMesh(pScene, pScene->mRootNode, identity, meshTransforms);

	for (uint32 i = 0; i < pScene->mNumMeshes; ++i) {
		m_Meshes.push_back(LoadMesh(pScene->mMeshes[i], renderer));
	}

	m_GPUMesh = LoadGPUMesh(pScene, (uint32_t)pScene->mNumMeshes, renderer, meshlet);
	uint32_t texViewIndex = 14;
	auto loadTexture = [renderer, &texViewIndex](std::string &basepath, aiMaterial *mat, aiTextureType type) {
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
			tex->CreateView(renderer, texViewDesc);
			tex->SetViewIndex(texViewIndex++);
			renderer->uploadTextureMap.insert({ tex, textureData });
		} else {
			switch (type) {
				case aiTextureType_NORMAL_CAMERA:
					tex = renderer->GetDefaultNormalTexPtr();
					tex->m_isDefault = true;
					break;
				case aiTextureType_METALNESS:
				case aiTextureType_DIFFUSE_ROUGHNESS:
				default:
					tex = renderer->GetDefaultBlackTexPtr();
					tex->m_isDefault = true;
					break;
			}
		}
		return tex;
	};
	std::string dirPath = path.substr(0, path.find_last_of("/\\"));

	m_Materials.resize(pScene->mNumMaterials);
	for (uint32 i = 0; i < pScene->mNumMaterials; ++i) {
		Material &m = m_Materials[i];
		// taskflow.emplace([&m, &dirPath, &loadTexture, mat = pScene->mMaterials[i]]() {
		// 	m.m_BaseTexture = loadTexture(dirPath, mat, aiTextureType_BASE_COLOR);
		// });

		// taskflow.emplace([&m, &dirPath, &loadTexture, mat = pScene->mMaterials[i]]() {
		// 	m.m_NormalTexture = loadTexture(dirPath, mat, aiTextureType_NORMAL_CAMERA);
		// });

		// taskflow.emplace([&m, &dirPath, &loadTexture, mat = pScene->mMaterials[i]]() {
		// 	m.m_MetallicTexture = loadTexture(dirPath, mat, aiTextureType_METALNESS);
		// });
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
	executor.run(taskflow).wait();
}

std::unique_ptr<SubMesh> Mesh::LoadMesh(aiMesh *pMesh, Renderer *renderer, bool meshlet) {
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
	std::unique_ptr<SubMesh> pSubMesh = std::make_unique<SubMesh>();
	pSubMesh->m_meshlet.resize(1);
	if (meshlet) {
		const size_t max_vertices = 64;
		const size_t max_triangles = 124;

		float cone_weight = 0.25; // 0.25 is good for occlusion culling
		size_t max_meshlets = meshopt_buildMeshletsBound(indexCount, max_vertices, max_triangles);
		pSubMesh->m_meshlet[0].m_meshlets.resize(max_meshlets);
		std::vector<unsigned int> meshlet_vertices(max_meshlets * vertexCount);
		std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

		size_t meshlet_count = meshopt_buildMeshlets(pSubMesh->m_meshlet[0].m_meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), meshdata->indices.data(),
				indexCount, (float *)meshdata->m_vertexesData.data(), vertexCount, sizeof(utils::Vertex), max_vertices, max_triangles, cone_weight);

		SPDLOG_INFO("Meshlet count: {}", meshlet_count);

		pSubMesh->m_meshlet[0].m_clusters.resize(meshlet_count);
		pSubMesh->m_meshlet[0].m_bounds.resize(meshlet_count);

		for (size_t i = 0; i < meshlet_count; ++i) {
			const meshopt_Meshlet &meshlet = pSubMesh->m_meshlet[0].m_meshlets[i];

			meshopt_optimizeMeshlet(&meshlet_vertices[meshlet.vertex_offset], &meshlet_triangles[meshlet.triangle_offset], meshlet.triangle_count, meshlet.vertex_count);

			pSubMesh->m_meshlet[0].m_clusters[i].resize(meshlet.triangle_count * 3);
			pSubMesh->m_meshlet[0].m_cluster_total_size += (uint32_t)pSubMesh->m_meshlet[0].m_clusters[i].size();
			for (size_t j = 0; j < meshlet.triangle_count * 3; ++j) {
				pSubMesh->m_meshlet[0].m_clusters[i][j] = meshlet_vertices[meshlet.vertex_offset + meshlet_triangles[meshlet.triangle_offset + j]];
			}

			pSubMesh->m_meshlet[0].m_bounds[i] = meshopt_computeMeshletBounds(&meshlet_vertices[meshlet.vertex_offset], &meshlet_triangles[meshlet.triangle_offset],
					meshlet.triangle_count, (float *)meshdata->m_vertexesData.data(), (uint32_t)meshdata->m_vertexesData.size(), sizeof(utils::Vertex));
		}
		pSubMesh->m_indexCount = pSubMesh->m_meshlet[0].m_cluster_total_size;
		pSubMesh->m_indexbuffer = std::make_unique<Buffer>();
		pSubMesh->m_vertexbuffer = std::make_unique<Buffer>();

		uint32_t indicesAlignSize = pSubMesh->m_meshlet[0].m_cluster_total_size * sizeof(uint32_t);
		uint32_t vertexSize = static_cast<uint32_t>(helper::GetByteSizeOf(meshdata->m_vertexesData));

		nri::BufferDesc desc = {};
		desc.size = indicesAlignSize;
		desc.usage = nri::BufferUsageBits::VERTEX_BUFFER |
				nri::BufferUsageBits::INDEX_BUFFER;
		nri::BufferViewDesc viewDesc{};
		pSubMesh->m_indexbuffer->Create(renderer, desc, viewDesc);

		meshdata->indices.resize(pSubMesh->m_meshlet[0].m_cluster_total_size);
		meshdata->m_vertexesData = remappedVertices;
		uint32_t offset = 0;
		for (size_t i = 0; i < pSubMesh->m_meshlet[0].m_clusters.size(); i++) {
			memcpy(&meshdata->indices[offset], pSubMesh->m_meshlet[0].m_clusters[i].data(), pSubMesh->m_meshlet[0].m_clusters[i].size() * sizeof(uint32_t));
			offset += (uint32_t)pSubMesh->m_meshlet[0].m_clusters[i].size();
		}
		renderer->uploadIndexBufferMap.insert({ pSubMesh->m_indexbuffer, meshdata });
		renderer->uploadShadowIndexBufferMap.insert({ pSubMesh->m_vertexbuffer, meshdata });
	} else {
		meshdata->indices = remappedIndices;
		meshdata->shadow_indices = shadow_indices;
		meshdata->m_vertexesData = remappedVertices;
		pSubMesh->name = pMesh->mName.C_Str();
		pSubMesh->aabb = std::make_pair(glm::make_vec3((float *)&aabb.mMin.x), glm::make_vec3((float *)&aabb.mMax));
		// 计算center+extend格式的包围盒
		glm::vec3 min = glm::make_vec3((float *)&aabb.mMin.x);
		glm::vec3 max = glm::make_vec3((float *)&aabb.mMax);
		glm::vec3 center = (min + max) * 0.5f;
		glm::vec3 extent = (max - min) * 0.5f;
		pSubMesh->aabb2 = std::make_pair(center, extent);

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

		renderer->GetNRI().SetDebugName(pSubMesh->m_indexbuffer->GetBuffer(), "meshlet IndexBuffer");
		renderer->GetNRI().SetDebugName(pSubMesh->m_vertexbuffer->GetBuffer(), "meshlet VertexBuffer");

		renderer->uploadIndexBufferMap.insert({ pSubMesh->m_indexbuffer, meshdata });
		renderer->uploadShadowIndexBufferMap.insert({ pSubMesh->m_vertexbuffer, meshdata });
	}
	pSubMesh->m_materialID = pMesh->mMaterialIndex;
	return pSubMesh;
}

std::unique_ptr<SubMesh> Mesh::LoadGPUMesh(const aiScene *pScene, uint32_t numMeshes, Renderer *renderer, bool meshlet) {
	std::unique_ptr<SubMesh> pGPUMesh = std::make_unique<SubMesh>();
	std::vector<std::shared_ptr<utils::MeshData>> meshDatas = {};
	// pGPUMesh->m_meshlet.m_drawArgs.resize(10000);
	meshDatas.resize(numMeshes);

	uint64_t indicesDataTotalAlignedSize = 0u;
	uint64_t vertexDataTotalSize = 0u;

	uint32_t preBaseIndex = 0;
	uint32_t preBaseVertex = 0;

	uint32_t preIndexOffset = 0;
	uint32_t preVertexOffset = 0;

	if (meshlet) {
		pGPUMesh->m_meshlet.resize(numMeshes);
		for (uint32_t i = 0; i < numMeshes; ++i) {
			aiMesh *pMesh = pScene->mMeshes[i];
			std::shared_ptr<utils::MeshData> meshdata = std::make_shared<utils::MeshData>();
			meshdata->m_vertexesData.resize(pMesh->mNumVertices);
			meshdata->vertices.resize(pMesh->mNumVertices);
			meshdata->indices.resize(pMesh->mNumFaces * 3);

			// Vertices Data
			{
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
			}
			// Indices Data
			for (unsigned int j = 0; j != pMesh->mNumFaces; j++) {
				for (int k = 0; k != 3; k++) {
					meshdata->indices[j * 3 + k] = pMesh->mFaces[j].mIndices[k];
				}
			}

			size_t indexCount = meshdata->indices.size();
			size_t vertexCount = meshdata->m_vertexesData.size();

			// Cluster Indices Buffer Data
			const size_t max_vertices = 64;
			const size_t max_triangles = 124;

			float cone_weight = 0.25; // 0.25 is good for occlusion culling
			size_t max_meshlets = meshopt_buildMeshletsBound(indexCount, max_vertices, max_triangles);
			pGPUMesh->m_meshlet[i].m_meshlets.resize(max_meshlets);
			m_meshlets.resize(max_meshlets);
			std::vector<unsigned int> meshlet_vertices(max_meshlets * meshdata->vertices.size());
			std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

			size_t meshlet_count = (uint32_t)meshopt_buildMeshlets(pGPUMesh->m_meshlet[i].m_meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), meshdata->indices.data(),
					indexCount, (float *)meshdata->m_vertexesData.data(), (uint32_t)vertexCount, (uint32_t)sizeof(utils::Vertex), max_vertices, max_triangles, cone_weight);

			pGPUMesh->m_meshlet[i].m_clusters.resize(meshlet_count);
			pGPUMesh->m_meshlet[i].m_bounds.resize(meshlet_count);
			for (size_t j = 0; j < meshlet_count; ++j) {
				const meshopt_Meshlet &meshlet = pGPUMesh->m_meshlet[i].m_meshlets[j];

				meshopt_optimizeMeshlet(&meshlet_vertices[meshlet.vertex_offset], &meshlet_triangles[meshlet.triangle_offset], meshlet.triangle_count, meshlet.vertex_count);

				pGPUMesh->m_meshlet[i].m_clusters[j].resize(meshlet.triangle_count * 3);
				pGPUMesh->m_meshlet[i].m_cluster_total_size += (uint32_t)pGPUMesh->m_meshlet[i].m_clusters[j].size();
				for (size_t k = 0; k < meshlet.triangle_count * 3; ++k) {
					pGPUMesh->m_meshlet[i].m_clusters[j][k] = meshlet_vertices[meshlet.vertex_offset + meshlet_triangles[meshlet.triangle_offset + k]];
				}

				pGPUMesh->m_meshlet[i].m_bounds[j] = meshopt_computeMeshletBounds(&meshlet_vertices[meshlet.vertex_offset], &meshlet_triangles[meshlet.triangle_offset],
						meshlet.triangle_count, (float *)meshdata->m_vertexesData.data(), (uint32_t)meshdata->m_vertexesData.size(), sizeof(utils::Vertex));
			}

			meshdata->indices.resize(pGPUMesh->m_meshlet[i].m_cluster_total_size);
			uint32_t offset = 0;
			for (size_t j = 0; j < pGPUMesh->m_meshlet[i].m_clusters.size(); j++) {
				memcpy(&meshdata->indices[offset], pGPUMesh->m_meshlet[i].m_clusters[j].data(), pGPUMesh->m_meshlet[i].m_clusters[j].size() * sizeof(uint32_t));
				offset += (uint32_t)pGPUMesh->m_meshlet[i].m_clusters[j].size();
			}

			SPDLOG_INFO("Meshlet count: {}", meshlet_count);

			uint32_t indicesSize = static_cast<uint32_t>(helper::GetByteSizeOf(meshdata->indices));
			uint32_t vertexSize = static_cast<uint32_t>(helper::GetByteSizeOf(meshdata->m_vertexesData));

			indicesDataTotalAlignedSize += indicesSize;
			vertexDataTotalSize += vertexSize;

			for (uint32_t j = 0; j < pGPUMesh->m_meshlet[i].m_clusters.size(); ++j) {
				DrawArgs drawArgs = {};
				// if (preBaseVertex < 1)
				{
					drawArgs.base.indexNum = (uint32_t)pGPUMesh->m_meshlet[i].m_clusters[j].size();
					drawArgs.base.vertexNum = (uint32_t)meshdata->m_vertexesData.size();
					drawArgs.base.baseIndex = preBaseIndex;
					drawArgs.base.baseVertex = preBaseVertex;
				}

				pGPUMesh->m_meshlet[i].m_drawArgs.push_back(drawArgs);
				preBaseIndex += (uint32_t)pGPUMesh->m_meshlet[i].m_clusters[j].size();
			}
			meshDatas[i] = meshdata;
			preBaseVertex += (uint32_t)meshdata->m_vertexesData.size();
			pGPUMesh->m_meshlet[i].m_materialIndex = pMesh->mMaterialIndex;
		}
	} else {
		m_drawArgs.resize(numMeshes);

		for (uint32_t i = 0; i < numMeshes; ++i) {
			aiMesh *pMesh = pScene->mMeshes[i];
			std::shared_ptr<utils::MeshData> meshdata = std::make_shared<utils::MeshData>();
			meshdata->m_vertexesData.resize(pMesh->mNumVertices);
			meshdata->vertices.resize(pMesh->mNumVertices);
			meshdata->indices.resize(pMesh->mNumFaces * 3);

			// Indices Data
			{
				for (uint32 j = 0; j < pMesh->mNumFaces; ++j) {
					const aiFace &face = pMesh->mFaces[j];
					for (uint32 k = 0; k < 3; ++k) {
						assert(face.mNumIndices == 3);
						meshdata->indices[j * 3 + k] = face.mIndices[k];
					}
				}
			}

			// Vertices Data
			{
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
			}

			uint32_t indicesSize = static_cast<uint32_t>(helper::GetByteSizeOf(meshdata->indices));
			uint32_t indicesAlignSize = static_cast<uint32_t>(helper::Align(indicesSize, 32));
			uint32_t vertexSize = static_cast<uint32_t>(helper::GetByteSizeOf(meshdata->m_vertexesData));

			uint64_t indexOffset = preIndexOffset;
			uint64_t vertexOffset = preVertexOffset;

			DrawArgs drawArgs = {};
			drawArgs.offset.indexNum = (uint32_t)meshdata->indices.size();
			drawArgs.offset.vertexNum = (uint32_t)meshdata->m_vertexesData.size();
			drawArgs.offset.indexOffset = (uint32_t)indexOffset;
			drawArgs.offset.vertexOffset = (uint32_t)vertexOffset;
#if 1
			drawArgs.base.indexNum = (uint32_t)meshdata->indices.size();
			drawArgs.base.vertexNum = (uint32_t)meshdata->m_vertexesData.size();

			drawArgs.base.baseIndex = preBaseIndex;
			drawArgs.base.baseVertex = preBaseVertex;

#endif

			indicesDataTotalAlignedSize += indicesSize;
			vertexDataTotalSize += vertexSize;

			preIndexOffset += indicesSize;
			preVertexOffset += vertexSize;

			preBaseIndex += (uint32_t)meshdata->indices.size();
			preBaseVertex += (uint32_t)meshdata->m_vertexesData.size();

			meshDatas[i] = meshdata;
			m_drawArgs[i] = drawArgs;
		}
	}

	// indicesDataTotalAlignedSize = helper::Align(indicesDataTotalAlignedSize, 32);

	nri::BufferDesc vertexBufferDesc = {};
	vertexBufferDesc.size = vertexDataTotalSize;
	vertexBufferDesc.usage = nri::BufferUsageBits::VERTEX_BUFFER;
	nri::BufferViewDesc viewDesc{};

	pGPUMesh->m_vertexbuffer = std::make_unique<Buffer>();
	pGPUMesh->m_vertexbuffer->Create(renderer, vertexBufferDesc, viewDesc);
	renderer->GetNRI().SetDebugName(pGPUMesh->m_vertexbuffer->GetBuffer(), "GPUMesh VB");

	nri::BufferDesc indexBufferDesc = {};
	indexBufferDesc.size = indicesDataTotalAlignedSize;
	indexBufferDesc.usage = nri::BufferUsageBits::INDEX_BUFFER;

	pGPUMesh->m_indexbuffer = std::make_unique<Buffer>();
	pGPUMesh->m_indexbuffer->Create(renderer, indexBufferDesc, viewDesc);
	renderer->GetNRI().SetDebugName(pGPUMesh->m_indexbuffer->GetBuffer(), "GPUMesh IB");

	std::vector<uint8_t> geometryData(indicesDataTotalAlignedSize);
	for (uint32_t i = 0; i < meshDatas.size(); ++i) {
		memcpy(&geometryData[pGPUMesh->m_meshlet[i].m_drawArgs[0].base.baseIndex * sizeof(uint32_t)], meshDatas[i]->indices.data(), helper::GetByteSizeOf(meshDatas[i]->indices));
	}

	std::vector<uint8_t> vertexData(vertexDataTotalSize);
	for (uint32_t i = 0; i < meshDatas.size(); ++i) {
		memcpy(&vertexData[pGPUMesh->m_meshlet[i].m_drawArgs[0].base.baseVertex * sizeof(utils::Vertex)], meshDatas[i]->m_vertexesData.data(), helper::GetByteSizeOf(meshDatas[i]->m_vertexesData));
	}

	nri::BufferUploadDesc indexBufferUploadDesc = {};
	indexBufferUploadDesc.data = geometryData.data();
	indexBufferUploadDesc.buffer = pGPUMesh->m_indexbuffer->GetBuffer();
	indexBufferUploadDesc.after = { nri::AccessBits::INDEX_BUFFER };

	nri::BufferUploadDesc vertexBufferUploadDesc = {};
	vertexBufferUploadDesc.data = vertexData.data();
	vertexBufferUploadDesc.buffer = pGPUMesh->m_vertexbuffer->GetBuffer();
	vertexBufferUploadDesc.after = { nri::AccessBits::VERTEX_BUFFER };

	std::vector<nri::BufferUploadDesc> uploadDescs = { vertexBufferUploadDesc, indexBufferUploadDesc };

	NRI_ABORT_ON_FAILURE(renderer->GetNRI().UploadData(renderer->GetRenderQueue(), nullptr, 0,
			uploadDescs.data(), (uint32_t)uploadDescs.size()));

	return pGPUMesh;
}