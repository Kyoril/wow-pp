
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
					if (!alphas.empty())
					{
						for (size_t t = 0; t < alphas.size(); ++t)
						{
							Ogre::String tileTexName = tileName + "_" + Ogre::StringConverter::toString(t + 1);
							Ogre::TexturePtr tex;
							if (!Ogre::TextureManager::getSingleton().resourceExists(tileTexName))
							{
								// create a manually managed texture resource
								tex = Ogre::TextureManager::getSingleton().createManual(
									tileTexName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
									64, 64, 0, Ogre::PF_A8, Ogre::TU_DEFAULT);
							}
							else
							{
								// create a manually managed texture resource
								tex = Ogre::TextureManager::getSingleton().getByName(tileTexName);
							}

							// Save a reference of this
							m_texturePtrs.push_back(tex);

							Ogre::HardwarePixelBufferSharedPtr buffer = tex->getBuffer();
							Ogre::PixelBox pixelBox(64, 64, 1, Ogre::PF_A8, alphas[t].data());
							Ogre::Image::Box imageBox(0, 0, 64, 64);
							buffer->blitFromMemory(pixelBox, imageBox);
						}
					}

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
						p->setLightingEnabled(true);
						p->createTextureUnitState(textures[texIds[0]]);

						// Create the alpha passes
						for (unsigned int t = 0; t < alphas.size(); ++t)
						{
							p = mat->getTechnique(0)->createPass();
							p->setLightingEnabled(true);
							p->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);

							Ogre::String tileTexName = tileName + "_" + Ogre::StringConverter::toString(t + 1);

							Ogre::TextureUnitState *ts = p->createTextureUnitState(textures[texIds[t + 1]]); 
							ts = p->createTextureUnitState(tileTexName, 1);
							ts->setColourOperationEx(Ogre::LBX_SOURCE1, Ogre::LBS_CURRENT, Ogre::LBS_CURRENT);
							ts->setAlphaOperation(Ogre::LBX_MODULATE, Ogre::LBS_CURRENT, Ogre::LBS_TEXTURE);
							ts->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
						}
					}

					auto &tile = m_tiles(i, j);
					tile.reset(new TerrainTile(
						m_sceneMgr, 
						m_camera,
						tileName, 
						*this,
						mat,
						i,
						j,
						m_data.page->heights[tileIdx],
						m_data.page->normals[tileIdx]));

					m_sceneNode->attachObject(tile.get());
				}
			}
		}

		TerrainPage::~TerrainPage()
		{
			m_sceneNode->detachAllObjects();
			m_sceneNode->getParentSceneNode()->removeAndDestroyChild(m_sceneNode->getName());
			m_sceneNode = nullptr;
		}

		void TerrainPage::createTechnique(Ogre::MaterialPtr material)
		{
			// Build page name
			std::ostringstream stream;
			stream << "Page " << m_data.position;

			const String textureName = stream.str();

			Ogre::Technique *tech = nullptr;
			Ogre::Pass *pass = nullptr;
			Ogre::TextureUnitState* tex = nullptr;

			// Deferred Technique
			tech = material->createTechnique();

			// Default pass
			pass = tech->createPass();
			pass->setPolygonMode(Ogre::PM_SOLID);
			pass->setLightingEnabled(true);
			pass->setAmbient(Ogre::ColourValue(0.8f, 0.8f, 0.8f));
			pass->setDiffuse(Ogre::ColourValue(0.8f, 0.8f, 0.8f));
			pass->setSpecular(Ogre::ColourValue(0.25f, 0.25f, 0.25f));
			pass->setShininess(128.0f);
		}
	}
}
