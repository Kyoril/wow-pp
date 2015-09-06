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

#include "ogre_mpq_archive.h"
#include "log/default_log_levels.h"
#include <cassert>

namespace wowpp
{
	namespace editor
	{
		MPQArchive::MPQArchive(const Ogre::String &name)
			: Ogre::Archive(name, "MPQ")
		{
		}

		void MPQArchive::load()
		{
			// Load the MPQ archive
			if (!SFileOpenArchive(mName.c_str(), 0, 0, &m_mpqHandle))
			{
				// Could not open MPQ archive file
				ELOG("Could not load MPQ archive " << mName << "!");
				return;
			}

			//TODO: Iterate through all files
			SFILE_FIND_DATA findData;
			HANDLE hFind = SFileFindFirstFile(m_mpqHandle, "*", &findData, "");
			if (!hFind)
			{
				ELOG("Could not read file list from MPQ archive " << mName << "!");
				return;
			}

			do
			{
				Ogre::FileInfo info;
				info.archive = this;
				info.filename = Ogre::String(findData.cFileName);
				Ogre::StringUtil::toLowerCase(info.filename);
				info.path = "";
				info.compressedSize = findData.dwCompSize;
				info.uncompressedSize = findData.dwFileSize;
				m_fileList.push_back(info);

			} while (SFileFindNextFile(hFind, &findData));

			// Close find handle
			SFileFindClose(hFind);
		}

		void MPQArchive::unload()
		{
			m_fileList.clear();

			// Unload MPQ archive
			if (!SFileCloseArchive(m_mpqHandle))
			{
				ELOG("Could not unload MPQ archive " << mName << "!");
				return;
			}
		}

#if (OGRE_VERSION_MAJOR >= 1 && OGRE_VERSION_MINOR == 10)
		Ogre::DataStreamPtr MPQArchive::open(const Ogre::String &filename, bool readonly)
#else
        Ogre::DataStreamPtr MPQArchive::open(const Ogre::String &filename, bool readonly) const
#endif
		{
			// Open the file for reading
			HANDLE hFile;
			if (!SFileOpenFileEx(m_mpqHandle, filename.c_str(), SFILE_OPEN_FROM_MPQ, &hFile))
			{
				ELOG("Could not open file " << filename << " in MPQ archive " << mName << "!");
				return Ogre::DataStreamPtr();
			}

			// Get file size in bytes
			DWORD higherFileSize = 0;
			DWORD fileSize = SFileGetFileSize(hFile, &higherFileSize);
			if (fileSize == SFILE_INVALID_SIZE)
			{
				ELOG("Could not determine size of file " << filename << " in MPQ archive " << mName);
				SFileCloseFile(hFile);
				return Ogre::DataStreamPtr();
			}
			
			// Will be freed automatically by ogre
			char *buffer = OGRE_ALLOC_T(char, fileSize, Ogre::MEMCATEGORY_GENERAL);
			memset(buffer, 0, fileSize);

			DWORD bytesRead = 0;
			if (!SFileReadFile(hFile, buffer, fileSize, &bytesRead, NULL))
			{
				ELOG("Could not read file " << filename << " from MPQ archive " << mName << "!");
				SFileCloseFile(hFile);
				return Ogre::DataStreamPtr();
			}

			if (bytesRead != fileSize)
			{
				WLOG("Invalid number of bytes: Expected " << fileSize << "; Read: " << bytesRead);
			}

			// Close file
			SFileCloseFile(hFile);
			
			return Ogre::DataStreamPtr(OGRE_NEW Ogre::MemoryDataStream(buffer, fileSize, true, readonly));
		}

		Ogre::StringVectorPtr MPQArchive::list(bool recursive /*= true*/, bool dirs /*= false*/)
		{
			Ogre::StringVectorPtr ret = Ogre::StringVectorPtr(OGRE_NEW_T(Ogre::StringVector, Ogre::MEMCATEGORY_GENERAL)(), Ogre::SPFM_DELETE_T);

			Ogre::FileInfoList::iterator i, iend;
			iend = m_fileList.end();

			for (i = m_fileList.begin(); i != iend; ++i)
			{
				if (!dirs)
				{
					// Found
					ret->push_back(i->filename);
				}
			}

			return ret;
		}

		Ogre::FileInfoListPtr MPQArchive::listFileInfo(bool recursive /*= true*/, bool dirs /*= false*/)
		{
			Ogre::FileInfoList *fil = OGRE_NEW_T(Ogre::FileInfoList, Ogre::MEMCATEGORY_GENERAL)();
			Ogre::FileInfoList::const_iterator i, iend;
			iend = m_fileList.end();

			for (i = m_fileList.begin(); i != iend; ++i)
			{
				if (!dirs)
				{
					fil->push_back(*i);
				}
			}

			return Ogre::FileInfoListPtr(fil, Ogre::SPFM_DELETE_T);
		}

		Ogre::StringVectorPtr MPQArchive::find(const Ogre::String &pattern, bool recursive /*= true*/, bool dirs /*= false*/)
		{
			Ogre::StringVectorPtr ret = Ogre::StringVectorPtr(OGRE_NEW_T(Ogre::StringVector, Ogre::MEMCATEGORY_GENERAL)(), Ogre::SPFM_DELETE_T);

			// If pattern contains a directory name, do a full match
			//	bool full_match = (pattern.find ('/') != Ogre::String::npos) ||
			//					  (pattern.find ('\\') != Ogre::String::npos);

			Ogre::FileInfoList::iterator i, iend;
			iend = m_fileList.end();
			for (i = m_fileList.begin(); i != iend; ++i)
			{
				if (pattern == "*")
				{
					// Found
					ret->push_back(i->filename);
				}
				else {
					// Starts with *?
					Ogre::String temp = pattern;
					Ogre::StringUtil::toLowerCase(temp);
					Ogre::String lCaseName = i->filename;
					Ogre::StringUtil::toLowerCase(lCaseName);

					if (Ogre::StringUtil::startsWith(temp, "*"))
					{
						// Ends with <pattern> (.material etc.)?
						if (Ogre::StringUtil::endsWith(temp, lCaseName))
						{
							// Add file definition
							ret->push_back(i->filename);
						}
					}
				}
			}

			return ret;
		}

#if (OGRE_VERSION_MAJOR >= 1 && OGRE_VERSION_MINOR == 10)
		Ogre::FileInfoListPtr MPQArchive::findFileInfo(const Ogre::String &pattern, bool recursive /*= true*/, bool dirs /*= false*/)
#else 
        Ogre::FileInfoListPtr MPQArchive::findFileInfo(const Ogre::String &pattern, bool recursive /*= true*/, bool dirs /*= false*/) const
#endif
		{
			Ogre::FileInfoListPtr ret = Ogre::FileInfoListPtr(OGRE_NEW_T(Ogre::FileInfoList, Ogre::MEMCATEGORY_GENERAL)(), Ogre::SPFM_DELETE_T);

			auto iend = m_fileList.end();
			for (auto i = m_fileList.begin(); i != iend; ++i)
			{
				if (!dirs)
				{
					if (Ogre::StringUtil::match(i->filename, pattern, false))
					{
						// Log
						ret->push_back(*i);
					}
				}
			}

			return ret;
		}

		bool MPQArchive::exists(const Ogre::String &filename)
		{
			for (Ogre::FileInfoList::iterator i = m_fileList.begin(); i != m_fileList.end(); ++i)
			{
				if (i->filename == filename || Ogre::StringUtil::match(i->filename, "*/" + filename, false))
				{
					return true;
				}
			}

			return false;
		}

		time_t MPQArchive::getModifiedTime(const Ogre::String &filename)
		{
			//TODO
			return 0;
		}



		const Ogre::String & MPQArchiveFactory::getType() const
		{
			static Ogre::String type("MPQ");
			return type;
		}

#if (OGRE_VERSION_MAJOR > 1) || (OGRE_VERSION_MAJOR == 1 && OGRE_VERSION_MINOR > 7)
		Ogre::Archive * MPQArchiveFactory::createInstance(const Ogre::String &name, bool readOnly)
#else
		Ogre::Archive * MPQArchiveFactory::createInstance(const Ogre::String &name)
#endif
		{
			return new MPQArchive(name);
		}

		void MPQArchiveFactory::destroyInstance(Ogre::Archive *arch)
		{
			OGRE_DELETE arch;
		}
	}
}
