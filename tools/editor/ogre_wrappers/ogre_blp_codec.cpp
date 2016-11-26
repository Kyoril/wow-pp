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
#include "ogre_blp_codec.h"
#include "OgreLogManager.h"
#include "OgreException.h"
#include "OgreRoot.h"
#include "OgreRenderSystem.h"
#include "OgreRenderSystemCapabilities.h"

namespace wowpp
{
	namespace editor
	{
		BLPCodec* BLPCodec::ms_instance = 0;

		void BLPCodec::startup(void)
		{
			if (!ms_instance)
			{
				Ogre::LogManager::getSingleton().logMessage(
					Ogre::LML_NORMAL,
					"BLP codec registering");

				ms_instance = OGRE_NEW BLPCodec();
				Ogre::Codec::registerCodec(ms_instance);
			}

		}

		void BLPCodec::shutdown(void)
		{
			if(ms_instance)
			{
				Ogre::Codec::unregisterCodec(ms_instance);
				OGRE_DELETE ms_instance;
				ms_instance = 0;
			}

		}

		BLPCodec::BLPCodec() 
			: m_type("blp")
		{ 
		}

		Ogre::DataStreamPtr BLPCodec::encode(Ogre::MemoryDataStreamPtr& input, Ogre::Codec::CodecDataPtr& pData) const
		{        
			OGRE_EXCEPT(Ogre::Exception::ERR_NOT_IMPLEMENTED,
				"BLP encoding not supported",
				"BLPCodec::code" ) ;
		}

		void BLPCodec::encodeToFile(Ogre::MemoryDataStreamPtr& input,
			const Ogre::String& outFileName, Ogre::Codec::CodecDataPtr& pData) const
		{
			
		}

		Ogre::Codec::DecodeResult BLPCodec::decode(Ogre::DataStreamPtr& input) const
		{
			// Read the header
			BLP2Header header;
			if (input->read(&header, sizeof(header)) != sizeof(header))
			{
				OGRE_EXCEPT(Ogre::Exception::ERR_INVALIDPARAMS,
					"This is not a BLP file!", "BLPCodec::decode");
			}

			Ogre::ImageCodec::ImageData* imgData = OGRE_NEW Ogre::ImageCodec::ImageData();
			Ogre::MemoryDataStreamPtr output;

			imgData->depth = 1;
			imgData->width = header.width;
			imgData->height = header.height;
			imgData->num_mipmaps = 1;	//Corrected later
			imgData->flags = 0;
			imgData->format = Ogre::PF_UNKNOWN;

			// Detect how many mipmaps ara available
			UInt32 prevSize = 0;
			for (UInt32 i = 1; i < 16; ++i)
			{
				if (header.mipmap_offsets[i] > 0/* &&
					prevSize != header.mipmap_length[i]*/)
				{
					prevSize = header.mipmap_length[i];
					imgData->num_mipmaps++;
				}
			}

			// Pixel format
			if (header.compression == 1)	// BLP Compression
			{
				OGRE_EXCEPT(Ogre::Exception::ERR_INVALIDPARAMS,
					"BLP compression is unsupported right now!", "BLPCodec::decode");
			}
			else if (header.compression == 2)	// DXT Compression
			{
				switch (header.alpha_type)
				{
					case 0:
					{
						imgData->format = Ogre::PixelFormat::PF_DXT1;
						imgData->flags |= Ogre::IF_COMPRESSED;
						break;
					}

					case 1:
					{
						imgData->format = Ogre::PixelFormat::PF_DXT3;
						imgData->flags |= Ogre::IF_COMPRESSED;
						break;
					}

					case 7:
					{
						imgData->format = Ogre::PixelFormat::PF_DXT5;
						imgData->flags |= Ogre::IF_COMPRESSED;
						break;
					}

					default:
					{
						OGRE_EXCEPT(Ogre::Exception::ERR_INVALIDPARAMS,
							"Unsupported alpha_type in BLP", "BLPCodec::decode");
					}
				}
			}
			else if (header.compression == 3)	// "Uncompressed"
			{
				imgData->format = Ogre::PixelFormat::PF_A8R8G8B8;
			}

			if (imgData->format == Ogre::PixelFormat::PF_UNKNOWN)
			{
				OGRE_EXCEPT(Ogre::Exception::ERR_INVALIDPARAMS,
					"Unknown pixel format", "BLPCodec::decode");
			}

			// Check if the hardware supports texture compression
			if (!Ogre::Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(Ogre::RSC_TEXTURE_COMPRESSION_DXT))
			{
				// It does not - manually decode?
				OGRE_EXCEPT(Ogre::Exception::ERR_INVALIDPARAMS,
					"Hardware does not support DXT format", "BLPCodec::decode");
			}

			// Calculate total size from number of mipmaps, faces and size
			imgData->size = Ogre::Image::calculateSize(imgData->num_mipmaps, 1,
				imgData->width, imgData->height, imgData->depth, imgData->format);

			// Bind output buffer
			output.bind(OGRE_NEW Ogre::MemoryDataStream(imgData->size));

			// Now deal with the data
			void* destPtr = output->getPtr();

			UInt32 mipW = header.width;
			UInt32 mipH = header.height;
			for (size_t i = 0; i < imgData->num_mipmaps; ++i)
			{
				// Resize buffer
				input->seek(header.mipmap_offsets[i]);
				input->read(destPtr, header.mipmap_length[i]);
				destPtr = static_cast<void*>(static_cast<unsigned char*>(destPtr) + header.mipmap_length[i]);

				if (mipW > 1) mipW /= 2;
				if (mipH > 1) mipH /= 2;
			}

			imgData->num_mipmaps--;

			// Return result
			Ogre::Codec::DecodeResult ret;
			ret.first = output;
			ret.second = Ogre::Codec::CodecDataPtr(imgData);
			return ret;
		}

		Ogre::String BLPCodec::magicNumberToFileExt(const char *magicNumberPtr, size_t maxbytes) const
		{
			if (maxbytes >= sizeof(UInt32))
			{
				// Reinterpret file type
				UInt32 fileType = *reinterpret_cast<const UInt32*>(magicNumberPtr);
				return Ogre::String("blp");
			}

			return Ogre::String();
		}

		Ogre::String BLPCodec::getType() const
		{
			return m_type;
		}
	}
}
