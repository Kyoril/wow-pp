//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "graphics.h"
#include "OgreColourValue.h"
#include "OgreMaterialManager.h"
#include <cassert>

namespace wowpp
{
	namespace editor
	{
		Graphics::Graphics()
			: m_root(new Ogre::Root("", "", "Ogre.log"))
			, m_glPlugin(new Ogre::GLPlugin())
			, m_sceneMgr(nullptr)
			, m_renderView(nullptr)
		{
			// Install render system plugin
			m_glPlugin->install();

			// Choose default render system, which should be OpenGL in our case
			assert(!m_root->getAvailableRenderers().empty());
			const auto &renderer = m_root->getAvailableRenderers().begin();
			m_root->setRenderSystem(*renderer);

			// Initialize the root object
			m_root->initialise(false);

			Ogre::MaterialManager::getSingleton().setDefaultTextureFiltering(Ogre::TFO_TRILINEAR);
			
			// Create our scene manager
			m_sceneMgr = m_root->createSceneManager(Ogre::ST_EXTERIOR_REAL_FAR);

			// Configure the scene manager
			m_sceneMgr->setAmbientLight(Ogre::ColourValue(0.3f, 0.3f, 0.3f));
			m_sceneMgr->setFog(Ogre::FOG_NONE);
		}
	}
}
