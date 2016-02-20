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

#include "game_incoming_packet.h"
#include "common/endian_convert.h"
#include <limits>

namespace wowpp
{
	namespace game
	{
		IncomingPacket::IncomingPacket()
			: m_id(std::numeric_limits<UInt16>::max())
			, m_size(0)
		{
		}

		ReceiveState IncomingPacket::start(IncomingPacket &packet, io::MemorySource &source)
		{
			io::Reader streamReader(source);

			if (streamReader
			        >> io::read<NetUInt16>(packet.m_size))
			{
				EndianConvertReverse(packet.m_size);

				if (source.getRest() >= packet.m_size)
				{
					if (streamReader
					        >> io::read<NetUInt32>(packet.m_id))
					{
						const char *const body = source.getPosition();
						streamReader.skip(packet.m_size - sizeof(NetUInt32));
						packet.m_body = io::MemorySource(body, body + packet.m_size - sizeof(NetUInt32));
						packet.setSource(&packet.m_body);
						return receive_state::Complete;
					}
				}
			}

			return receive_state::Incomplete;
		}
	}
}
