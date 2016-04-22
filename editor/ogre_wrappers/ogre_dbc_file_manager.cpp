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
#include "ogre_dbc_file_manager.h"

template<> wowpp::OgreDBCFileManager* Ogre::Singleton<wowpp::OgreDBCFileManager>::msSingleton = nullptr;

namespace wowpp
{
	OgreDBCFileManager::OgreDBCFileManager()
	{
		mResourceType = "OgreDBCFile";

		// low, because it will likely reference other resources
		mLoadOrder = 30.0f;

		// this is how we register the ResourceManager with OGRE
		Ogre::ResourceGroupManager::getSingleton()._registerResourceManager(mResourceType, this);
	}
	OgreDBCFileManager::~OgreDBCFileManager()
	{
		// and this is how we unregister it
		Ogre::ResourceGroupManager::getSingleton()._unregisterResourceManager(mResourceType);
	}
	OgreDBCFilePtr OgreDBCFileManager::load(const Ogre::String & name, const Ogre::String & group)
	{
		OgreDBCFilePtr pDbc = createOrRetrieve(name, group, false, 0, 0).first.staticCast<OgreDBCFile>();
		pDbc->load();
		return pDbc;
	}
	Ogre::Resource * OgreDBCFileManager::createImpl(const Ogre::String & name, Ogre::ResourceHandle handle, const Ogre::String & group, bool isManual, Ogre::ManualResourceLoader * loader, const Ogre::NameValuePairList * createParams)
	{
		return OGRE_NEW OgreDBCFile(this, name, handle, group, isManual, loader);
	}
}
