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

#include "ogre_dbc_file.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	OgreDBCFile::OgreDBCFile(
		Ogre::ResourceManager * creator, 
		const Ogre::String & name, 
		Ogre::ResourceHandle handle, 
		const Ogre::String & group, 
		bool isManual, 
		Ogre::ManualResourceLoader * loader)
		: Ogre::Resource(creator, name, handle, group, isManual, loader)
		, m_recordCount(0)
		, m_fieldCount(0)
		, m_recordSize(0)
		, m_stringSize(0)
		, m_recordOffset(0)
		, m_stringOffset(0)
	{
		createParamDictionary("OgreDBCFile");
	}
	OgreDBCFile::~OgreDBCFile()
	{
		unload();
	}
	void OgreDBCFile::loadImpl()
	{
		Ogre::DataStreamPtr stream = Ogre::ResourceGroupManager::getSingleton().openResource(mName, mGroup, true, this);

		// Read four-cc code
		UInt32 fourcc = 0;
		stream->read(&fourcc, sizeof(UInt32));
		if (fourcc != 0x43424457)
		{
			// No need to parse any more - invalid CC
			OGRE_EXCEPT(Ogre::Exception::ERR_INTERNAL_ERROR, "File is not a valid DBC file!", __FUNCTION__);
		}

		bool result = (
			stream->read(&m_recordCount, sizeof(UInt32)) &&
			stream->read(&m_fieldCount, sizeof(UInt32)) &&
			stream->read(&m_recordSize, sizeof(UInt32)) &&
			stream->read(&m_stringSize, sizeof(UInt32))
			);
		if (!result)
		{
			OGRE_EXCEPT(Ogre::Exception::ERR_INTERNAL_ERROR, "Could not read DBC header", __FUNCTION__);
		}

		if (m_fieldCount * 4 != m_recordSize)
		{
			OGRE_EXCEPT(Ogre::Exception::ERR_INTERNAL_ERROR, "DBC field cound does not match record size!", __FUNCTION__);
		}

		// Calculate data offsets
		m_recordOffset = stream->tell();
		m_stringOffset = m_recordOffset + (m_recordCount * m_recordSize);

		ILOG("Loaded " << m_recordCount << " entries in dbc file");

		// Read data columns
		m_data.resize(m_recordCount);
		for (UInt32 record = 0; record < m_recordCount; ++record)
		{
			auto &rec = m_data[record];
			rec.resize(m_fieldCount);
			for (UInt32 field = 0; field < m_fieldCount; ++field)
			{
				stream->read(&rec[field], sizeof(UInt32));
				if (field == 0)
				{
					m_rowsByIndex[rec[field]] = record;
				}
			}
		}

		// Look for the string data
		stream->seek(m_stringOffset);
		m_stringData.resize(m_stringSize);
		stream->read(&m_stringData[0], m_stringSize);
	}
	void OgreDBCFile::unloadImpl()
	{
		m_data.clear();

		m_recordCount = 0;
		m_fieldCount = 0;
		m_recordSize = 0;
		m_stringSize = 0;
		m_recordOffset = 0;
		m_stringOffset = 0;
	}
	size_t OgreDBCFile::calculateSize() const
	{
		return (m_recordSize * m_recordCount) + m_stringSize;
	}
	UInt32 OgreDBCFile::getRowByIndex(UInt32 index) const
	{
		auto it = m_rowsByIndex.find(index);
		if (it == m_rowsByIndex.end())
		{
			return UInt32(-1);
		}

		return it->second;
	}
	const String OgreDBCFile::getField(UInt32 row, UInt32 column) const
	{
		UInt32 stringOffset = getField<UInt32>(row, column);
		if (stringOffset == 0) return String();

		return String(&m_stringData[stringOffset]);
	}
}
