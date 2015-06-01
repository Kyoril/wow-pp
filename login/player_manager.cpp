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

#include "player_manager.h"
#include "player.h"
#include "binary_io/string_sink.h"
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

	Player * PlayerManager::getPlayerByAccountName(const String &accountName)
	{
		const auto p = std::find_if(
			m_players.begin(),
			m_players.end(),
			[&accountName](const std::unique_ptr<Player> &p)
		{
			return (p->isAuthentificated() &&
				accountName == p->getAccountName());
		});

		if (p != m_players.end())
		{
			return (*p).get();
		}

		return nullptr;
	}

}
