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
#include "game_outgoing_packet.h"
#include "common/macros.h"
#include "game_crypt.h"
#include "common/endian_convert.h"

namespace wowpp
{
	namespace game
	{
		OutgoingPacket::OutgoingPacket(io::ISink &sink, bool isProxy/* = false*/)
			: io::Writer(sink)
			, m_opCode(0)
			, m_size(0)
			, m_proxy(isProxy)
		{
		}

		void OutgoingPacket::start(UInt16 id)
		{
			m_opCode = id;

			if (!m_proxy)
			{
				m_sizePosition = sink().position();

				*this
				        << io::write<NetUInt16>(0);		// Size

				m_bodyPosition = sink().position();

				UInt16 convertedCmd = id;
				EndianConvert(convertedCmd);

				*this
				        << io::write<NetUInt16>(convertedCmd);	// Packet-ID
			}
		}

		void OutgoingPacket::finish()
		{
			if (!m_proxy)
			{
				const std::size_t end = sink().position();
				ASSERT(end >= m_bodyPosition);

				m_size = (end - m_bodyPosition);
				ASSERT(m_size <= std::numeric_limits<NetUInt16>::max());

				UInt16 convertedSize = static_cast<UInt16>(m_size);
				EndianConvertReverse(convertedSize);

				writePOD(m_sizePosition, static_cast<UInt16>(convertedSize));
			}
		}
	}
}
