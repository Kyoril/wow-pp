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

#pragma once

#include "OgreArchive.h"
#include "OgreArchiveFactory.h"
#include "OgreString.h"
#include "stormlib/src/StormLib.h"

namespace wowpp
{
	namespace editor
	{
		/// Explains the MPQ file format to OGRE so we are able to load WoW files straight
		/// from the MPQ archives.
		class MPQArchive final : public Ogre::Archive
		{
		public:

			/// 
			explicit MPQArchive(const Ogre::String &name);

			/// @copydoc Ogre::Archive::load()
			void load() override;
			/// @copydoc Ogre::Archive::unload()
			void unload() override;
			/// @copydoc Ogre::Archive::open()
			Ogre::DataStreamPtr open(const Ogre::String &filename, bool readonly) const override;
			/// @copydoc Ogre::Archive::list()
			Ogre::StringVectorPtr list(bool recursive = true, bool dirs = false) override;
			/// @copydoc Ogre::Archive::listFileInfo()
			Ogre::FileInfoListPtr listFileInfo(bool recursive = true, bool dirs = false) override;
			/// @copydoc Ogre::Archive::find()
			Ogre::StringVectorPtr find(const Ogre::String &pattern, bool recursive = true, bool dirs = false) override;
#if (OGRE_VERSION_MAJOR > 1) || (OGRE_VERSION_MAJOR == 1 && OGRE_VERSION_MINOR > 7)
			/// @copydoc Ogre::Archive::findFileInfo()
			Ogre::FileInfoListPtr findFileInfo(const Ogre::String &pattern, bool recursive = true, bool dirs = false) const override;
#else
			/// @copydoc Ogre::Archive::findFileInfo()
			Ogre::FileInfoListPtr findFileInfo(const Ogre::String &pattern, bool recursive = true, bool dirs = false) override;
#endif
			/// @copydoc Ogre::Archive::exists()
			bool exists(const Ogre::String &filename) override;
			/// @copydoc Ogre::Archive::getModifiedTime()
			time_t getModifiedTime(const Ogre::String &filename) override;
			/// @copydoc Ogre::Archive::isCaseSensitive()
			bool isCaseSensitive() const override { return false; }

		private:

			Ogre::FileInfoList m_fileList;
			HANDLE m_mpqHandle;
		};

		/// Class which is used to register our custom Archive class at OGRE's Archive
		/// manager.
		class MPQArchiveFactory final : public Ogre::ArchiveFactory
		{
		public:

			/// @copydoc Ogre::ArchiveFactory::getType()
			const Ogre::String &getType() const override;
#if (OGRE_VERSION_MAJOR > 1) || (OGRE_VERSION_MAJOR == 1 && OGRE_VERSION_MINOR > 7)
			/// @copydoc Ogre::ArchiveFactory::createInstance()
			Ogre::Archive *createInstance(const Ogre::String &name, bool readOnly) override;
#else
			/// @copydoc Ogre::ArchiveFactory::createInstance()
			Ogre::Archive *createInstance(const Ogre::String &name) override;
#endif
			/// @copydoc Ogre::ArchiveFactory::destroyInstance()
			void destroyInstance(Ogre::Archive *arch) override;
		};
	}
}
