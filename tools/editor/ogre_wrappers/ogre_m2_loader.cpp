//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#include "pch.h"
#include "ogre_m2_loader.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace editor
	{
		void M2MeshLoader::loadResource(Ogre::Resource * resource)
		{
			// Load mesh resource
			Ogre::Mesh* mesh = dynamic_cast<Ogre::Mesh*>(resource);
			if (!mesh)
			{
				OGRE_EXCEPT(Ogre::Exception::ERR_INTERNAL_ERROR, "M2MeshLoader can only load Ogre::Mesh resource objects", "M2MeshLoader::loadResource");
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

				std::vector<String> textureNames;
				for (auto &texture : textures)
				{
					if (texture.fileNameOffs > 0 && texture.fileNameLen > 1)
					{
						ptr->seek(texture.fileNameOffs);

						String name(texture.fileNameLen - 1, '\0');
						ptr->read(&name[0], texture.fileNameLen - 1);

						textureNames.push_back(name);
					}
					else
					{
						textureNames.push_back("");
					}
				}

				if (header.nVertices == 0 || header.nViews == 0)
				{
					OGRE_EXCEPT(Ogre::Exception::ERR_INVALID_STATE, "M2MeshLoader: No mesh data found", "M2MeshLoader::loadResource");
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
					//*(base + 1) = 1.0f - *(base + 1);
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

					DLOG("View 0 ntex: " << view.nTextures);

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
						
						String textureToUse;
						if (!textureNames.empty())
						{
							UInt32 texIndex = i % textureNames.size();
							textureToUse = textureNames[i];
						}

						if (!textureToUse.empty())
						{
							auto *texUnit = pass->createTextureUnitState();
							texUnit->setTextureName(textureToUse);
						}
						else
						{
							pass->setDiffuse(0.0f, 1.0f, 0.0f, 1.0f);
						}

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
	}
}
