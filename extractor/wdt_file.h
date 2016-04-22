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
	/// This class contains data from a wdt file.
	class WDTFile final : public MPQFile
	{
	public:

		/// Stores data from the MAIN chunk.
		struct ChunkMAIN final
		{
			UInt32 fourCC;
			UInt32 size;
			struct ADTData
			{
				UInt32 exist;
				UInt32 data1;

				ADTData()
					: exist(0)
					, data1(0)
				{
				}
			};
			std::array<ADTData, 64 * 64> adt;

			/// Initializes the data of this chunk to zero.
			ChunkMAIN()
				: fourCC(0)
				, size(0)
			{
			}
		};

		/// Stores data from the MPHD chunk.
		struct ChunkMPHD final
		{
			UInt32 fourCC;
			UInt32 size;
			UInt32 data1;
			UInt32 data2;
			UInt32 data3;
			UInt32 data4;
			UInt32 data5;
			UInt32 data6;
			UInt32 data7;
			UInt32 data8;

			/// Initializes the data of this chunk to zero.
			ChunkMPHD()
				: fourCC(0)
				, size(0)
				, data1(0)
				, data2(0)
				, data3(0)
				, data4(0)
				, data5(0)
				, data6(0)
				, data7(0)
				, data8(0)
			{
			}
		};

		/// Stores data from the MWMO chunk.
		struct ChunkMWMO final
		{
			UInt32 fourCC;
			UInt32 size;

			/// Initializes the data of this chunk to zero.
			ChunkMWMO()
				: fourCC(0)
				, size(0)
			{	  
			}
		};

	public:

		/// @copydoc MPQFile::MPQFile()
		explicit WDTFile(String fileName);

		/// @copydoc MPQFile::load
		bool load() override;

		/// Gets the MAIN chunk informations of the wdt file.
		const ChunkMAIN &getMAINChunk() const { return m_mainChunk; }
		/// Gets the MPHD chunk informations of the wdt file.
		const ChunkMPHD &getMPHDChunk() const { return m_headerChunk; }
		/// Gets the MWMO chunk informations of the wdt file.
		const ChunkMWMO &getMWMOChunk() const { return m_wmoChunk; }

	private:

		bool m_isValid;
		ChunkMAIN m_mainChunk;
		ChunkMPHD m_headerChunk;
		ChunkMWMO m_wmoChunk;
	};
}
