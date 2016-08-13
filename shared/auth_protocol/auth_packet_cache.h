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

#include "auth_outgoing_packet.h"
#include "binary_io/vector_sink.h"
#include "binary_io/writer.h"

namespace wowpp
{
	namespace auth
	{
		template <class F>
		class PacketCache
		{
		public:

			explicit PacketCache(F createPacket)
				: m_createPacket(createPacket)
			{
			}

			void copyToSink(io::ISink &sink)
			{
				if (m_buffer.empty())
				{
					io::VectorSink bufferSink(m_buffer);
					wowpp::auth::OutgoingPacket packet(bufferSink);
					m_createPacket(packet);
					assert(!m_buffer.empty());
				}

				io::Writer sinkWriter(sink);
				sinkWriter << io::write_range(m_buffer);
				sink.flush();
			}

		private:

			F m_createPacket;
			std::vector<char> m_buffer;
		};


		template <class F>
		PacketCache<F> makePacketCache(F createPacket)
		{
			return PacketCache<F>(createPacket);
		}
	}
}
