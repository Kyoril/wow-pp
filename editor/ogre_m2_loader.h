
#pragma once

#include "common/typedefs.h"
#include <OgreHardwareBuffer.h>
#include <OgreStringConverter.h>
#include <OgreDataStream.h>
#include <OgreMesh.h>
#include <OgreSubMesh.h>
#include <OgreResourceGroupManager.h>
#include <OgreHardwareVertexBuffer.h>
#include <OgreVertexIndexData.h>
#include <OgreHardwareBufferManager.h>
#include <OgreException.h>
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>

namespace wowpp
{
	namespace editor
	{
		struct M2Header
		{
			UInt32 magic;
			UInt32 version;
			UInt32 nameLen;
			UInt32 nameOffset;
			UInt32 globalFlags;
			UInt32 nGlobalSequences;
			UInt32 ofsGlobalSequences;
			UInt32 nAnimations;
			UInt32 ofsAnimations;
			UInt32 nAnimationLookup;
			UInt32 ofsAnimationLookup;
			UInt32 nPlayableAnimationLookup;
			UInt32 ofsPlayableAnimationLookup;
			UInt32 nBones;
			UInt32 ofsBones;
			UInt32 nSkelBoneLookup;
			UInt32 ofsSkelBoneLookup;
			UInt32 nVertices;
			UInt32 ofsVertices;
			UInt32 nViews;
			UInt32 ofsViews;
			UInt32 nColors;
			UInt32 ofsColors;
			UInt32 nTextures;
			UInt32 ofsTextures;
			UInt32 nTransparency;
			UInt32 ofsTransparency;
			UInt32 nI;
			UInt32 ofsI;
			UInt32 nTexAnims;
			UInt32 ofsTexAnims;
			UInt32 nTexReplace;
			UInt32 ofsTexReplace;
			UInt32 nRenderFlags;
			UInt32 ofsRenderFlags;
			UInt32 nBoneLookupTable;
			UInt32 ofsBoneLookupTable;
			UInt32 nTexLookup;
			UInt32 ofsTexLookup;
			UInt32 nTexUnits;
			UInt32 ofsTexUnits;
			UInt32 nTransLookup;
			UInt32 ofsTransLookup;
			UInt32 nTexAnimLookup;
			UInt32 ofsTexAnimLookup;
			std::array<float, 14> values;
			UInt32 nBoundingTriangles;
			UInt32 ofsBoundingTriangles;
			UInt32 nBoundingVertices;
			UInt32 ofsBoundingVertices;
			UInt32 nBoundingNormals;
			UInt32 ofsBoundingNormals;
			UInt32 nAttachments;
			UInt32 ofsAttachments;
			UInt32 nAttachLookup;
			UInt32 ofsAttachLookup;
			UInt32 nAttachments_2;
			UInt32 ofsAttachments_2;
			UInt32 nLights;
			UInt32 ofsLights;
			UInt32 nCameras;
			UInt32 ofsCameras;
			UInt32 nCameraLookup;
			UInt32 ofsCameraLookup;
			UInt32 nRibbonEmitters;
			UInt32 ofsRibbonEmitters;
			UInt32 nParticleEmitters;
			UInt32 ofsParticleEmitters;
		};

		struct M2View
		{
			UInt32 nIndices;
			UInt32 ofsIndices;
			UInt32 nTriangles;
			UInt32 ofsTriangles;
			UInt32 nVertexProperties;
			UInt32 ofsVertexProperties;
			UInt32 nSubMeshs;
			UInt32 ofsSubMeshs;
			UInt32 nTextures;
			UInt32 ofsTextures;
		};

		struct M2SubMesh
		{
			UInt32 id;
			UInt16 startVert;
			UInt16 nVerts;
			UInt16 startTri;
			UInt16 nTris;
			UInt16 nBones;
			UInt16 startBone;
			UInt16 unkBone1;
			UInt16 rootBone;
			float vec1[3];
			float vec2[4];
		};

		typedef UInt16 M2Index;

		struct M2Texture
		{
			UInt32 type;
			UInt16 unk1;
			UInt16 flags;
			UInt32 fileNameLen;
			UInt32 fileNameOffs;
		};

		class M2MeshLoader : public Ogre::ManualResourceLoader
		{
		private:

			Ogre::String m_fileName;

		public:

			M2MeshLoader(Ogre::String fileName)
				: m_fileName(std::move(fileName))
			{
			}

			virtual void loadResource(Ogre::Resource* resource) override
			{
				// Load mesh resource
				Ogre::Mesh* mesh = dynamic_cast<Ogre::Mesh*>(resource);
				if (!mesh)
				{
					OGRE_EXCEPT(Ogre::Exception::ERR_INVALID_CALL, "M2MeshLoader can only load Ogre::Mesh resource objects", "M2MeshLoader::loadResource");
				}

				Ogre::DataStreamPtr ptr = Ogre::ResourceGroupManager::getSingleton().openResource(m_fileName, "WoW", false);
				if (!ptr.isNull())
				{
					M2Header header;
					ptr->read(&header, sizeof(M2Header));
					if (header.magic != 0x3032444D)
					{
						OGRE_EXCEPT(Ogre::Exception::ERR_INVALID_STATE, "M2MeshLoader: Invalid m2 mesh header", "M2MeshLoader::loadResource");
					}

					// Read model name
					Ogre::String modelName(header.nameLen - 1, '\0');
					ptr->seek(header.nameOffset);
					ptr->readLine(&modelName[0], header.nameLen - 1, "\0");

					// Create and assign vertex data object
					Ogre::VertexData *vertData = new Ogre::VertexData();
					mesh->sharedVertexData = vertData;
					vertData->vertexStart = 0;
					vertData->vertexCount = header.nVertices;

					// Read texture definitions
					std::vector<M2Texture> textures(header.nTextures);
					if (header.ofsTextures && header.nTextures)
					{
						ptr->seek(header.ofsTextures);
						ptr->read(&textures[0], sizeof(M2Texture) * header.nTextures);
					}

					// Setup vertex attribute declaration
					Ogre::VertexDeclaration* decl = vertData->vertexDeclaration;
					size_t offset = decl->addElement(0, 0, Ogre::VET_FLOAT3, Ogre::VES_POSITION).getSize();
					offset += decl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_NORMAL).getSize();
					offset += decl->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 0).getSize();

					// Create the vertex buffer and write vertex data
					Ogre::HardwareVertexBufferSharedPtr vbuf = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
						decl->getVertexSize(0),
						header.nVertices,
						Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY
						);

					// Bind the newly created vertex buffer
					Ogre::VertexBufferBinding* bind = vertData->vertexBufferBinding;
					bind->setBinding(0, vbuf);

					Ogre::Vector3 vMin(99999.0f, 99999.0f, 99999.0f);
					Ogre::Vector3 vMax(-99999.0f, -99999.0f, -99999.0f);

					std::vector<float> verts(8 * header.nVertices, 0.0f);
					float *base = &verts[0];

					// Skip to the specified position
					ptr->seek(header.ofsVertices);
					for (UInt32 i = 0; i < header.nVertices; ++i)
					{
						// Read position data
						ptr->read(base, sizeof(float) * 3);
						if (*base < vMin.x) vMin.x = *base;
						if (*(base + 1) < vMin.y) vMin.y = *(base + 1);
						if (*(base + 2) < vMin.z) vMin.z = *(base + 2);
						if (*base > vMax.x) vMax.x = *base;
						if (*(base + 1) > vMax.y) vMax.y = *(base + 1);
						if (*(base + 2) > vMax.z) vMax.z = *(base + 2);
						base += 3;

						// Skip bone data
						ptr->skip(sizeof(unsigned char) * 8);

						// Read normal data
						ptr->read(base, sizeof(float) * 3);
						base += 3;

						// Read uv data and invert v
						ptr->read(base, sizeof(float) * 2);
						*(base + 1) = 1.0f - *(base + 1);
						base += 2;

						// Skip unknown data
						ptr->skip(sizeof(float) * 2);
					}

					vbuf->writeData(0, verts.size() * sizeof(float), &verts[0], true);

					// Read views
					if (header.nViews > 0)
					{
						// Seek view offset
						ptr->seek(header.ofsViews);

						// Read first view
						M2View view;
						ptr->read(&view, sizeof(M2View));

						// Read index list
						std::vector<M2Index> indices(view.nIndices, 0);
						ptr->seek(view.ofsIndices);
						ptr->read(&indices[0], sizeof(M2Index) * view.nIndices);

						// Read triangle list
						std::vector<M2Index> triangles(view.nTriangles);
						ptr->seek(view.ofsTriangles);
						ptr->read(&triangles[0], sizeof(M2Index) * view.nTriangles);

						// Create the index buffer and fill it with data
						Ogre::HardwareIndexBufferSharedPtr ibuf = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
							Ogre::HardwareIndexBuffer::IT_16BIT,
							view.nTriangles,
							Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY
							);
						unsigned short* base = static_cast<unsigned short*>(ibuf->lock(0, ibuf->getSizeInBytes(), Ogre::HardwareBuffer::HBL_NORMAL));
						{
							for (UInt16 j = 0; j < view.nTriangles; ++j)
							{
								M2Index ind = indices[triangles[j]];
								*base++ = ind;
							}
						}
						ibuf->unlock();

						// Load all submeshs
						ptr->seek(view.ofsSubMeshs);
						for (UInt32 i = 0; i < view.nSubMeshs; ++i)
						{
							// Load submesh properties
							M2SubMesh sub;
							ptr->read(&sub, sizeof(M2SubMesh));

							const Ogre::String materialName = modelName + "_" + Ogre::StringConverter::toString(i);
							Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().create(materialName, "WoW");
							mat->removeAllTechniques();

							auto *teq = mat->createTechnique();
							teq->removeAllPasses();

							auto *pass = teq->createPass();
							pass->removeAllTextureUnitStates();
							pass->setSceneBlending(Ogre::SBF_ONE, Ogre::SBF_ONE_MINUS_DEST_ALPHA);

							auto *texUnit = pass->createTextureUnitState();
							texUnit->setTextureName("CREATURE\\Kobold\\koboldskinNormal.blp");

							// Create a new submesh
							Ogre::SubMesh *subMesh = mesh->createSubMesh();
							subMesh->useSharedVertices = true;
							subMesh->indexData = new Ogre::IndexData();
							subMesh->indexData->indexStart = sub.startTri;
							subMesh->indexData->indexCount = sub.nTris;
							subMesh->indexData->indexBuffer = ibuf;
							subMesh->setMaterialName(materialName);
						}
					}

					mesh->_setBounds(Ogre::AxisAlignedBox(vMin, vMax));
					mesh->_setBoundingSphereRadius(std::max(vMax.x - vMin.x, std::max(vMax.y - vMin.y, vMax.z - vMin.z)) / 2.0f);
				}
			}
		};
	}
}