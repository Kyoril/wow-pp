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

#pragma once

#include "common/typedefs.h"
#include "common/vector.h"
#include "terrain_editing.h"
#include "terrain_page.h"
#include "ogre_wrappers/entity_ptr.h"
#include "ogre_wrappers/scene_node_ptr.h"

namespace wowpp
{
	namespace view
	{
		class WorldRenderer : public boost::noncopyable
		{
		public:

			explicit WorldRenderer(Ogre::SceneManager &sceneMgr, Ogre::Camera &camera);
			
			void handleEvent(const terrain::editing::TerrainChangeEvent &event);
			void update(float delta);

		private:

			typedef std::shared_ptr<TerrainPage> TerrainPagePtr;
			typedef std::map<terrain::model::PagePosition, TerrainPagePtr> TerrainPages;

			Ogre::SceneManager &m_sceneMgr;
			Ogre::Camera &m_camera;
			TerrainPages m_terrainPages;
		};
	}
}
