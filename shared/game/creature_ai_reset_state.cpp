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

#include "creature_ai_reset_state.h"
#include "creature_ai.h"
#include "game_creature.h"
#include "world_instance.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include "each_tile_in_sight.h"
#include "common/constants.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	CreatureAIResetState::CreatureAIResetState(CreatureAI &ai)
		: CreatureAIState(ai)
		, m_moveUpdate(ai.getControlled().getTimers())
	{
		m_moveUpdate.ended.connect([this]()
		{
			auto &ai = getAI();
			const auto &pos = ai.getHome().position;

			getControlled().relocate(pos[0], pos[1], pos[2], ai.getHome().orientation);
			ai.idle();
		});
	}

	CreatureAIResetState::~CreatureAIResetState()
	{
	}

	void CreatureAIResetState::onEnter()
	{
		const float distance = getControlled().getDistanceTo(getAI().getHome().position);

		// TODO: Make the creature return to it's home
		GameTime moveTime = (distance / 7.5f) * constants::OneSecond;

		// Send move packet
		TileIndex2D tile;
		if (getControlled().getTileIndex(tile))
		{
			float o;
			Vector<float, 3> oldPosition;
			getControlled().getLocation(oldPosition[0], oldPosition[1], oldPosition[2], o);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::monsterMove(packet, getControlled().getGuid(), oldPosition, getAI().getHome().position, moveTime);

			forEachSubscriberInSight(
				getControlled().getWorldInstance()->getGrid(),
				tile,
				[&packet, &buffer](ITileSubscriber &subscriber)
			{
				subscriber.sendPacket(packet, buffer);
			});
		}

		m_moveUpdate.setEnd(getCurrentTime() + moveTime);
	}

	void CreatureAIResetState::onLeave()
	{

	}

}
