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

#include "mpq_file.h"

namespace wowpp
{
	/// This class contains data from a dbc file.
	class DBCFile final : public MPQFile
	{
	public:

		/// 
		explicit DBCFile(String fileName);

		/// @copydoc MPQFile::load()
		bool load() override;

		/// Gets the number of rows in this dbc file.
		UInt32 getRecordCount() const { return m_recordCount; }
		/// Gets the number of fields in this dbc file.
		UInt32 getFieldCount() const { return m_fieldCount; }

		template<class T>
		bool getValue(UInt32 row, UInt32 column, T &out_value)
		{
			if (!m_isValid) return false;
			if (row >= m_recordCount || column >= m_fieldCount) return false;

			// Go to that position
			const size_t rowOffset = row * m_recordSize;
			const size_t colOffset = column * 4;
			m_source->seek(m_recordOffset + rowOffset + colOffset);

			// Read value
			return m_reader
				>> io::read<T>(out_value);
		}
		bool getValue(UInt32 row, UInt32 column, String &out_value)
		{
			UInt32 stringOffset = 0;
			if (!getValue<UInt32>(row, column, stringOffset))
			{
				return false;
			}

			m_source->seek(m_stringOffset + stringOffset);

			out_value.clear();
			char c = 0;
			do
			{
				if (!(m_reader >> io::read<char>(c)))
				{
					return false;
				}

				if (c != 0) out_value.push_back(c);
			} while (c != 0);

			return true;
		}

	private:

		bool m_isValid;
		UInt32 m_recordCount;
		UInt32 m_fieldCount;
		UInt32 m_recordSize;
		UInt32 m_stringSize;
		size_t m_recordOffset;
		size_t m_stringOffset;
	};
}
