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

#include "single_cast_state.h"
#include "game_unit.h"
#include "no_cast_state.h"
#include "common/clock.h"
#include "log/default_log_levels.h"
#include "world_instance.h"
#include "visibility_grid.h"
#include "visibility_tile.h"
#include "each_tile_in_sight.h"
#include "binary_io/vector_sink.h"
#include "game_protocol/game_protocol.h"
#include <boost/iterator/indirect_iterator.hpp>

namespace wowpp
{
	SingleCastState::SingleCastState(SpellCast &cast, const SpellEntry &spell, SpellTargetMap target, GameTime castTime)
		: m_cast(cast)
		, m_spell(spell)
		, m_target(std::move(target))
		, m_hasFinished(false)
		, m_countdown(cast.getTimers())
		, m_isAlive(std::make_shared<char>(0))
	{
		// Check if the executer is in the world
		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();
		if (!worldInstance)
		{
			WLOG("Spell cast executer is not part of any world instance!");
			return;
		}

		m_countdown.ended.connect([this]()
		{
			this->onCastFinished();
		});

		m_countdown.setEnd(getCurrentTime() + castTime);

		//if (castTime > 0)
		{
			auto const casterId = executer.getGuid();
			auto const targetId = target.getUnitTarget();
			auto const spellId = spell.id;
			
			float x, y, z, o;
			executer.getLocation(x, y, z, o);

			TileIndex2D tileIndex;
			worldInstance->getGrid().getTilePosition(x, y, z, tileIndex[0], tileIndex[1]);
			
			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::spellStart(packet, casterId, casterId, m_spell, m_target, game::spell_cast_flags::Unknown1, castTime, 0);

			forEachSubscriberInSight(
				worldInstance->getGrid(),
				tileIndex,
				[&buffer, &packet](ITileSubscriber &subscriber)
			{
				subscriber.sendPacket(
					packet,
					buffer
					);
			});
		}

		// TODO: Subscribe to target removed and died events (in both cases, the cast may be interrupted)
		
		// TODO: Subscribe to damage events if the spell is cancelled on damage

	}

	std::pair<game::SpellCastResult, SpellCasting *> SingleCastState::startCast(SpellCast &cast, const SpellEntry &spell, SpellTargetMap target, GameTime castTime, bool doReplacePreviousCast)
	{
		if (!m_hasFinished &&
			!doReplacePreviousCast)
		{
			return std::make_pair(game::spell_cast_result::FailedSpellInProgress, &m_casting);
		}

		SpellTargetMap targetMap = m_target;
		SpellCasting &casting = castSpell(
			m_cast,
			m_spell,
			targetMap,
			castTime);

		return std::make_pair(game::spell_cast_result::CastOkay, &casting);
	}

	void SingleCastState::stopCast()
	{
		m_countdown.cancel();

		if (!m_hasFinished)
		{
			sendEndCast(false);
			m_hasFinished = true;
		}

		const std::weak_ptr<char> isAlive = m_isAlive;
		m_casting.ended(false);

		if (isAlive.lock())
		{
			m_cast.setState(std::unique_ptr<SpellCast::CastState>(
				new NoCastState
				));
		}
	}

	void SingleCastState::onUserStartsMoving()
	{
		// TODO: Interrupt spell cast if moving
	}

	void SingleCastState::sendEndCast(bool success)
	{
		DLOG("Spell cast for " << m_spell.id << " finished! Success: " << success);

		auto &executer = m_cast.getExecuter();
		auto *worldInstance = executer.getWorldInstance();
		if (!worldInstance)
		{
			WLOG("Spell cast executer is not part of any world instance!");
			return;
		}

		if (success)
		{
			float x, y, z, o;
			executer.getLocation(x, y, z, o);

			TileIndex2D tileIndex;
			worldInstance->getGrid().getTilePosition(x, y, z, tileIndex[0], tileIndex[1]);

			std::vector<char> buffer;
			io::VectorSink sink(buffer);
			game::Protocol::OutgoingPacket packet(sink);
			game::server_write::spellGo(packet, executer.getGuid(), executer.getGuid(), m_spell, m_target, game::spell_cast_flags::Unknown3);

			forEachSubscriberInSight(
				worldInstance->getGrid(),
				tileIndex,
				[&buffer, &packet](ITileSubscriber &subscriber)
			{
				subscriber.sendPacket(
					packet,
					buffer
					);
			});
		}
		else
		{
			// Spell cast failed...
		}
	}

	void SingleCastState::onCastFinished()
	{
		// TODO: We need to check some conditions now

		// TODO: Consume power

		// TODO: Apply cooldown

		sendEndCast(true);
		m_hasFinished = true;

		const std::weak_ptr<char> isAlive = m_isAlive;

		// TODO: Apply spell effects on all targets
		
		if (isAlive.lock())
		{
			//may destroy this, too
			m_casting.ended(true);
		}
	}

	void SingleCastState::onTargetRemovedOrDead()
	{
		stopCast();
	}

	void SingleCastState::onUserDamaged()
	{
		// This is only triggerd if the spell has the attribute
		stopCast();
	}
}
