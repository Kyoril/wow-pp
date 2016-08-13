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
#include "wowpp_outgoing_packet.h"
#include "common/constants.h"

namespace wowpp
{
	namespace pp
	{
		OutgoingPacket::OutgoingPacket(io::ISink &sink)
			: io::Writer(sink)
		{
		}

		void OutgoingPacket::start(PacketId id)
		{
			*this
			        << io::write<NetPacketBegin>(constants::PacketBegin)
			        << io::write<NetUInt8>(id);

			m_sizePosition = sink().position();

			*this
			        << io::write<NetPacketSize>(0);

			m_bodyPosition = sink().position();
		}

		void OutgoingPacket::finish()
		{
			const std::size_t end = sink().position();
			assert(end >= m_bodyPosition);

			const std::size_t size = (end - m_bodyPosition);
			assert(size <= std::numeric_limits<NetPacketSize>::max());

			writePOD(m_sizePosition, static_cast<NetPacketSize>(size));
		}
	}
}
