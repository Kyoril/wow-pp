
#include "terrain_page.h"
#include "terrain_tile.h"
#include "OgreSceneNode.h"
#include "OgreTextureManager.h"
#include "OgreMaterialManager.h"
#include "OgreStringConverter.h"
#include "OgreTechnique.h"
#include "OgrePass.h"
#include "game/constants.h"
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

			// Build page name
			std::ostringstream stream;
			stream << "Page " << m_data.position;
			const String name = stream.str();

			// Create the texture
			if (Ogre::TextureManager::getSingleton().resourceExists(name))
			{
				Ogre::TextureManager::getSingleton().remove(name);
			}

			// create a manually managed texture resource
			m_terrainTexture = Ogre::TextureManager::getSingleton().createManual(
				name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
				512, 512, 1, 0, Ogre::PF_BYTE_RGBA, Ogre::TU_STATIC_WRITE_ONLY);

			// Create material if needed
			if (!Ogre::MaterialManager::getSingleton().resourceExists(name))
			{
				// Create material
				m_material = Ogre::MaterialManager::getSingleton().create(name,
					Ogre::ResourceGroupManager::getSingleton().DEFAULT_RESOURCE_GROUP_NAME, false);
			}
			else
			{
				m_material = Ogre::MaterialManager::getSingleton().getByName(name);
			}

			// Remove all techniques
			m_material->removeAllTechniques();

			// Create material technique
			createTechnique(m_material);

			// Tiles laden
			m_tiles = TileGrid(constants::TilesPerPage, constants::TilesPerPage);
			for (unsigned int i = 0; i < constants::TilesPerPage; i++)
			{
				for (unsigned int j = 0; j < constants::TilesPerPage; j++)
				{
					Ogre::String tileName = m_sceneNode->getName() + "_Tile_" + Ogre::StringConverter::toString(i) + "_" + Ogre::StringConverter::toString(j);

					auto &tile = m_tiles(i, j);
					tile.reset(new TerrainTile(
						m_sceneMgr, 
						m_camera,
						tileName, 
						*this, 
						m_material, 
						i,
						j,
						m_data.page->heights[j + i * constants::TilesPerPage]));

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
			pass->setPolygonMode(Ogre::PM_WIREFRAME);
			pass->setLightingEnabled(true);
			pass->setAmbient(Ogre::ColourValue(0.8f, 0.8f, 0.8f));
			pass->setDiffuse(Ogre::ColourValue(0.8f, 0.8f, 0.8f));
			pass->setSpecular(Ogre::ColourValue(0.25f, 0.25f, 0.25f));
			pass->setShininess(128.0f);
		}
	}
}
