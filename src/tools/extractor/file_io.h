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

namespace wowpp
{
	/// Class taken from SampleInterfaces of RecastDemo application. Used to dump
	/// navigation data into obj files.
	class FileIO : public duFileIO
	{
	private:

		// Explicitly disabled copy constructor and copy assignment operator.
		FileIO(const FileIO&) = delete;
		FileIO& operator=(const FileIO&) = delete;

	public:

		/// Default constructor
		FileIO();
		/// Destructor
		virtual ~FileIO();

		/// Opens the file handle for writing.
		/// @param path Name of the file to open.
		/// @returns False if something went wrong.
		bool openForWrite(const char* path);
		/// Opens the file handle for reading.
		/// @param path Name of the file to open.
		/// @returns False if something went wrong.
		bool openForRead(const char* path);

		/// @copydoc duFileIO::isWriting()
		virtual bool isWriting() const override;
		/// @copydoc duFileIO::isReading()
		virtual bool isReading() const override;
		/// @copydoc duFileIO::write(const void* ptr, const size_t size)
		virtual bool write(const void* ptr, const size_t size) override;
		/// @copydoc duFileIO::read(void* ptr, const size_t size)
		virtual bool read(void* ptr, const size_t size) override;

	private:

		FILE* m_fp;
		int m_mode;
	};
}
