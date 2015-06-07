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

#include "common/typedefs.h"
#include "OgreImageCodec.h"

namespace wowpp
{
	namespace editor
	{
		struct BLP2Header
		{
			UInt8 ident[4];
			UInt32 type;
			UInt8 compression;
			UInt8 alpha_depth;
			UInt8 alpha_type;
			UInt8 has_mips;
			UInt32 width;
			UInt32 height;
			UInt32 mipmap_offsets[16];
			UInt32 mipmap_length[16];
			UInt32 palette[256];
		};


		/// Explains the BLP file format to OGRE so we are able to load WoW BLP files and
		/// render them.
		class BLPCodec final : public Ogre::ImageCodec
		{
		private:

			Ogre::String m_type;

			/// Single registered codec instance
			static BLPCodec* ms_instance;

		public:

			BLPCodec();

			/// @copydoc Ogre::Codec::code
			Ogre::DataStreamPtr code(Ogre::MemoryDataStreamPtr& input, Ogre::Codec::CodecDataPtr& pData) const override;
			/// @copydoc Ogre::Codec::codeToFile
			void codeToFile(Ogre::MemoryDataStreamPtr& input, const Ogre::String& outFileName, Ogre::Codec::CodecDataPtr& pData) const override;
			/// @copydoc Ogre::Codec::decode
			Ogre::Codec::DecodeResult decode(Ogre::DataStreamPtr& input) const override;
			/// @copydoc Ogre::Codec::magicNumberToFileExt
			Ogre::String magicNumberToFileExt(const char *magicNumberPtr, size_t maxbytes) const override;

			/// @copydoc Ogre::Codec::getType
			virtual Ogre::String getType() const override;

			/// Static method to startup and register the BLP codec
			static void startup();
			/// Static method to shutdown and unregister the BLP codec
			static void shutdown();
		};
	}
}
