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
#include <OgreResourceManager.h>

namespace wowpp
{
	class OgreDBCFile : public Ogre::Resource
	{
	public:

		OgreDBCFile(Ogre::ResourceManager *creator, const Ogre::String &name,
			Ogre::ResourceHandle handle, const Ogre::String &group, bool isManual = false,
			Ogre::ManualResourceLoader *loader = 0);
		virtual ~OgreDBCFile();

		/// Gets the number of rows of this dbc file.
		UInt32 getRowCount() const { return m_recordCount; }
		/// Gets the number of columns of this dbc file.
		UInt32 getColumnCount() const { return m_fieldCount; }

		UInt32 getRowByIndex(UInt32 index) const;

		template<class T>
		T getField(UInt32 row, UInt32 column) const;
		const String getField(UInt32 row, UInt32 column) const;

	protected:

		void loadImpl() override;
		void unloadImpl() override;
		size_t calculateSize() const override;

	private:

		UInt32 m_recordCount;
		UInt32 m_fieldCount;
		UInt32 m_recordSize;
		UInt32 m_stringSize;
		size_t m_recordOffset;
		size_t m_stringOffset;
		std::vector<std::vector<UInt32>> m_data;
		std::vector<char> m_stringData;
		std::map<UInt32, UInt32> m_rowsByIndex;
	};

	typedef Ogre::SharedPtr<OgreDBCFile> OgreDBCFilePtr;

	template<class T>
	inline T OgreDBCFile::getField(UInt32 row, UInt32 column) const
	{
		return m_data[row][column];
	}
}
