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

#include "player_manager.h"
#include "player.h"
#include "binary_io/string_sink.h"
#include "log/default_log_levels.h"
#include "game/each_tile_in_sight.h"
#include <algorithm>
#include <cassert>

namespace wowpp
{
	PlayerManager::PlayerManager(
	    size_t playerCapacity)
		: m_playerCapacity(playerCapacity)
	{
	}

	PlayerManager::~PlayerManager()
	{
	}

	void PlayerManager::playerDisconnected(Player &player)
	{
		const auto p = std::find_if(
		                   m_players.begin(),
		                   m_players.end(),
		                   [&player](const std::unique_ptr<Player> &p)
		{
			return (&player == p.get());
		});
		assert(p != m_players.end());
		m_players.erase(p);
	}

	const PlayerManager::Players &PlayerManager::getPlayers() const
	{
		return m_players;
	}
	
	bool PlayerManager::hasPlayerCapacityBeenReached() const
	{
		return m_players.size() >= m_playerCapacity;
	}

	void PlayerManager::addPlayer(std::unique_ptr<Player> added)
	{
		assert(added);
		m_players.push_back(std::move(added));
	}

	Player *PlayerManager::getPlayerByCharacterId(DatabaseId id)
	{
		const auto p = std::find_if(
			m_players.begin(),
			m_players.end(),
			[id](const std::unique_ptr<Player> &p)
		{
			return (id == p->getCharacterId());
		});

		if (p == m_players.end())
		{
			return nullptr;
		}

		return p->get();
	}

	Player *PlayerManager::getPlayerByCharacterGuid(UInt64 guid)
	{
		const auto p = std::find_if(
			m_players.begin(),
			m_players.end(),
			[guid](const std::unique_ptr<Player> &p)
		{
			return (guid == p->getCharacterGuid());
		});

		if (p == m_players.end())
		{
			return nullptr;
		}

		return p->get();
	}
}
