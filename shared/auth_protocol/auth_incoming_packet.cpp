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

#include "auth_incoming_packet.h"
#include <limits>

namespace wowpp
{
	namespace auth
	{
		IncomingPacket::IncomingPacket()
			: m_id(std::numeric_limits<UInt8>::max())
		{
		}

		UInt8 IncomingPacket::getId() const
		{
			return m_id;
		}

		ReceiveState IncomingPacket::start(IncomingPacket &packet, io::MemorySource &source)
		{
			io::Reader streamReader(source);
			
			if (streamReader
				>> io::read<NetUInt8>(packet.m_id))
			{
				std::size_t size = source.getRest();
				/*if (source.getRest() >= size)
				{*/
					const char *const body = source.getPosition();
					source.skip(size);
					packet.m_body = io::MemorySource(body, body + size);
					packet.setSource(&packet.m_body);
					return receive_state::Complete;
				//}
			}

			return receive_state::Incomplete;
		}
	}
}
