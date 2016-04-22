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
#include "OgreCamera.h"
#include "game/constants.h"
#include "log/default_log_levels.h"
#include "common/clock.h"

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
				(63 - data.position[0]) * constants::MapWidth - (constants::MapWidth * 32.0f),
				(63 - data.position[1]) * constants::MapWidth - (constants::MapWidth * 32.0f),
				0.0f
				);

			m_tilesLoaded = 0;
			m_tiles = TileGrid(constants::TilesPerPage, constants::TilesPerPage);
		}

		TerrainPage::~TerrainPage()
		{
			for (auto &tile : m_tiles)
			{
				if (tile)
				{
					const auto &tileName = tile->getName();
					if (Ogre::TextureManager::getSingleton().resourceExists(tileName))
					{
						Ogre::TextureManager::getSingleton().remove(tileName);
					}
				}
			}

			m_sceneNode->removeAndDestroyAllChildren();
			m_sceneNode->getParentSceneNode()->removeAndDestroyChild(m_sceneNode->getName());
			m_sceneNode = nullptr;
		}

		void TerrainPage::update()
		{
			// Nothing to load here
			if (m_tilesLoaded >= constants::TilesPerPage * constants::TilesPerPage)
				return;

			const float scale = (constants::MapWidth / static_cast<float>(constants::TilesPerPage));
			const auto &camPos = m_camera.getDerivedPosition();

			std::vector<float> tileDistanceToCamera(constants::TilesPerPage * constants::TilesPerPage, -1.0f);
			for (UInt32 t = 0; t < tileDistanceToCamera.size(); ++t)
			{
				unsigned int j = t / constants::TilesPerPage;
				unsigned int i = t % constants::TilesPerPage;
				if (m_tiles(i, j))
				{
					// Tile is already loaded
					continue;
				}

				Ogre::Vector3 nodePosition(scale * (16 - j), scale * (16 - i), camPos.z);
				tileDistanceToCamera[t] = (camPos - nodePosition).length();
			}

			const UInt32 tilesToLoad = constants::TilesPerPage / 4;
			for (UInt32 t = 0; t < tilesToLoad; ++t)
			{
				float minDistance = 99999.0f;
				Int32 tileIndexToLoad = -1;
				for (UInt32 x = 0; x < tileDistanceToCamera.size(); ++x)
				{
					if (tileDistanceToCamera[x] == -1.0f)
						continue;

					if (tileDistanceToCamera[x] < minDistance)
					{
						minDistance = tileDistanceToCamera[x];
						tileIndexToLoad = x;
					}
				}

				assert(tileIndexToLoad >= 0);
				tileDistanceToCamera[tileIndexToLoad] = -1.0f;

				// Load the next tile
				unsigned int j = tileIndexToLoad / constants::TilesPerPage;
				unsigned int i = tileIndexToLoad % constants::TilesPerPage;
				m_tilesLoaded++;

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
				for (size_t tx = 0; tx < 64 * 64; ++tx)
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

				Ogre::SceneNode *tileNode = m_sceneNode->createChildSceneNode(
					Ogre::Vector3(scale * (16 - j), scale * (16 - i), 0.0f));

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
					m_data.page->normals[tileIdx],
					m_data.page->holes[tileIdx]));

				tileNode->attachObject(tile.get());
				//tileNode->showBoundingBox(true);
			}
		}
	}
}
