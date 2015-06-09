
#include "terrain_page.h"
#include "terrain_tile.h"
#include "OgreSceneNode.h"
#include "OgreTextureManager.h"
#include "OgreMaterialManager.h"
#include "OgreStringConverter.h"
#include "OgreTechnique.h"
#include "OgrePass.h"
#include "OgreHardwarePixelBuffer.h"
#include "OgreImage.h"
#include "OgrePixelFormat.h"
#include "game/constants.h"
#include "log/default_log_levels.h"
#include "common/clock.h"
#include <cassert>

namespace wowpp
{
	namespace view
	{
		TerrainPage::TerrainPage(
			const terrain::editing::PositionedPage &data,
			Ogre::SceneManager &sceneMgr,
			Ogre::Camera &camera,
			Ogre::SceneNode &worldNode)
			: m_sceneMgr(sceneMgr)
			, m_data(data)
			, m_camera(camera)
		{
			assert(m_data.page);

			m_sceneNode = worldNode.createChildSceneNode();
			m_sceneNode->setPosition(
				data.position[0] * constants::MapWidth - (constants::MapWidth * 32.0f),
				0.0f, 
				data.position[1] * constants::MapWidth - (constants::MapWidth * 32.0f)
				);

			// Tiles laden
			m_tiles = TileGrid(constants::TilesPerPage, constants::TilesPerPage);
			for (unsigned int i = 0; i < constants::TilesPerPage; i++)
			{
				for (unsigned int j = 0; j < constants::TilesPerPage; j++)
				{
					Ogre::String tileName = m_sceneNode->getName() + "_Tile_" + Ogre::StringConverter::toString(i) + "_" + Ogre::StringConverter::toString(j);

					const UInt32 tileIdx = j + i * constants::TilesPerPage;
					auto &textures = m_data.page->textures;
					auto &texIds = m_data.page->textureIds[tileIdx];
					auto &alphas = m_data.page->alphaMaps[tileIdx];

					// Create the alphamap textures
					Ogre::TexturePtr tex;
					if (!Ogre::TextureManager::getSingleton().resourceExists(tileName))
					{
						// create a manually managed texture resource
						tex = Ogre::TextureManager::getSingleton().createManual(
							tileName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
							64, 64, 0, Ogre::PF_B8G8R8A8, Ogre::TU_DEFAULT);	// BGRA because OpenGL wants it like this
					}
					else
					{
						// create a manually managed texture resource
						tex = Ogre::TextureManager::getSingleton().getByName(tileName);
					}

					Ogre::HardwarePixelBufferSharedPtr buffer = tex->getBuffer();
					buffer->lock(Ogre::HardwareBuffer::HBL_DISCARD);

					// Copy all image data
					const Ogre::PixelBox& pixelBox = buffer->getCurrentLock();
					UInt8* pDest = static_cast<UInt8*>(pixelBox.data);
					for (size_t tx = 0; tx < 64*64; ++tx)
					{
						*pDest++ = (alphas.size() > 2 ? alphas[2][tx] : 0);
						*pDest++ = (alphas.size() > 1 ? alphas[1][tx] : 0);
						*pDest++ = (alphas.size() > 0 ? alphas[0][tx] : 0);
						*pDest++ = 255;	// OpenGL wants an alpha bit
					}

					buffer->unlock();

					// Create a material for that tile
					Ogre::MaterialPtr mat;
					if (!Ogre::MaterialManager::getSingleton().resourceExists(tileName))
					{
						// Create material
						mat = Ogre::MaterialManager::getSingleton().create(tileName,
							Ogre::ResourceGroupManager::getSingleton().DEFAULT_RESOURCE_GROUP_NAME, false);
					}
					else
					{
						mat = Ogre::MaterialManager::getSingleton().getByName(tileName);
					}

					// Create material technique
					mat->removeAllTechniques();
					Ogre::Technique *tech = nullptr;
					tech = mat->createTechnique();
					if (!texIds.empty())
					{
						// Create the base pass
						Ogre::Pass *p = mat->getTechnique(0)->createPass();
						p->setVertexProgram("TerrainVP");
						p->setFragmentProgram("TerrainFP");
						Ogre::TextureUnitState *tex = p->createTextureUnitState(tileName);
						tex->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);

						// Add tile textures
						for (auto &tex : texIds)
						{
							p->createTextureUnitState(textures[tex]);
						}
					}

					const float scale = (constants::MapWidth / static_cast<float>(constants::TilesPerPage));
					Ogre::SceneNode *tileNode = m_sceneNode->createChildSceneNode(
						Ogre::Vector3(scale * j, 0.0f, scale * i));

					auto &tile = m_tiles(i, j);
					tile.reset(new TerrainTile(
						m_sceneMgr, 
						m_camera,
						tileName, 
						*this,
						mat,
						0,
						0,
						m_data.page->heights[tileIdx],
						m_data.page->normals[tileIdx]));

					tileNode->attachObject(tile.get());
					//tileNode->showBoundingBox(true);
					//m_sceneNode->showBoundingBox(true);
				}
			}
		}

		TerrainPage::~TerrainPage()
		{
			m_sceneNode->detachAllObjects();
			m_sceneNode->getParentSceneNode()->removeAndDestroyChild(m_sceneNode->getName());
			m_sceneNode = nullptr;
		}
	}
}
