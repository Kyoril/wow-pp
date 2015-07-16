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

#include "mpq_file.h"
#include <array>

namespace wowpp
{
	/// This class contains data from an adt file.
	class ADTFile final : public MPQFile
	{
	public:

		/// 
		struct MVERChunk final
		{
			UInt32 fourcc;
			UInt32 size;
			UInt32 version;

			/// 
			MVERChunk()
				: fourcc(0)
				, size(0)
				, version(0)
			{
			}
		};

		/// 
		struct MHDRChunk final
		{
			UInt32 fourcc;
			UInt32 size;
			UInt32 pad;
			UInt32 offsMCIN;           // MCIN
			UInt32 offsTex;            // MTEX
			UInt32 offsModels;         // MMDX
			UInt32 offsModelsIds;      // MMID
			UInt32 offsMapObejcts;     // MWMO
			UInt32 offsMapObejctsIds;  // MWID
			UInt32 offsDoodsDef;       // MDDF
			UInt32 offsObjectsDef;     // MODF
			UInt32 offsMFBO;           // MFBO
			UInt32 offsMH2O;           // MH2O
			UInt32 data1;
			UInt32 data2;
			UInt32 data3;
			UInt32 data4;
			UInt32 data5;

			/// 
			MHDRChunk()
				: fourcc(0)
				, size(0)
				, pad(0)
				, offsMCIN(0)
				, offsTex(0)
				, offsModels(0)
				, offsModelsIds(0)
				, offsMapObejcts(0)
				, offsMapObejctsIds(0)
				, offsDoodsDef(0)
				, offsObjectsDef(0)
				, offsMFBO(0)
				, offsMH2O(0)
				, data1(0)
				, data2(0)
				, data3(0)
				, data4(0)
				, data5(0)
			{
			}
		};

		/// 
		struct MCINChunk final
		{
			UInt32 fourcc;
			UInt32 size;

			/// 
			struct ADTCell
			{
				UInt32 offMCNK;
				UInt32 size;
				UInt32 flags;
				UInt32 asyncId;

				/// 
				ADTCell()
					: offMCNK(0)
					, size(0)
					, flags(0)
					, asyncId(0)
				{
				}
			};
			std::array<ADTCell, 16 * 16> cells;

			/// 
			MCINChunk()
				: fourcc(0)
				, size(0)
			{
			}
		};

	public:

		/// @copydoc MPQFile::MPQFile()
		explicit ADTFile(String fileName);

		/// @copydoc MPQFile::load
		bool load() override;

		/// 
		const MVERChunk &getMVERChunk() const { return m_versionChunk; }
		/// 
		const MHDRChunk &getMHDRChunk() const { return m_headerChunk; }
		/// 
		const MCINChunk &getMCINChunk() const { return m_mcinChunk; }

	private:

		bool m_isValid;
		MVERChunk m_versionChunk;
		MHDRChunk m_headerChunk;
		size_t m_offsetBase;
		MCINChunk m_mcinChunk;
	};
}
