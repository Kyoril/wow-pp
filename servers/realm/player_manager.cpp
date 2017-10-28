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
#include "player_manager.h"
#include "player.h"
#include "player_social.h"
#include "player_group.h"
#include "game/game_object.h"
#include "binary_io/string_sink.h"
#include "log/default_log_levels.h"
#include "common/timer_queue.h"

namespace wowpp
{
	PlayerManager::PlayerManager(
		TimerQueue &timers,
		UInt16 realmID,
	    size_t playerCapacity)
		: m_timers(timers)
		, m_realmID(realmID)
		, m_playerCapacity(playerCapacity)
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
		                   [&player](const PlayerPtr &p)
		{
			return (&player == p.get());
		});
		ASSERT(p != m_players.end());
		m_players.erase(p);
	}

	bool PlayerManager::hasPlayerCapacityBeenReached() const
	{
		return m_players.size() >= m_playerCapacity;
	}

	void PlayerManager::addPlayer(PlayerPtr added)
	{
		ASSERT(added);
		m_players.push_back(added);

		// Send SMSG_AUTH_CHALLENGE to client
		m_players.back()->sendAuthChallenge();
	}

	PlayerManager::PlayerPtr PlayerManager::getPlayerByAccountName(const String &accountName)
	{
		const auto p = std::find_if(
			m_players.begin(),
			m_players.end(),
			[&accountName](const PlayerPtr &p)
		{
			return (accountName == p->getAccountName());
		});

		if (p != m_players.end())
		{
			return *p;
		}

		return nullptr;
	}

	PlayerManager::PlayerPtr PlayerManager::getPlayerByCharacterId(DatabaseId id)
	{
		// Invalid guid
		if (id == 0)
		{
			return nullptr;
		}

		// Search for character
		const auto p = std::find_if(
			m_players.begin(),
			m_players.end(),
			[&id](const PlayerPtr &p)
		{
			return (id == p->getCharacterId());
		});

		if (p != m_players.end())
		{
			return *p;
		}

		return nullptr;
	}

	PlayerManager::PlayerPtr PlayerManager::getPlayerByCharacterGuid(UInt64 id)
	{
		// Invalid guid
		if (id == 0)
		{
			return nullptr;
		}

		// Search for character
		const auto p = std::find_if(
			m_players.begin(),
			m_players.end(),
			[&id](const PlayerPtr &p)
		{
			return (id == p->getWorldObjectId());
		});

		if (p != m_players.end())
		{
			return *p;
		}

		return nullptr;
	}

	PlayerManager::PlayerPtr PlayerManager::getPlayerByCharacterName(const String &name)
	{
		if (name.empty())
		{
			return nullptr;
		}

		// Search for character
		const auto p = std::find_if(
			m_players.begin(),
			m_players.end(),
			[&name](const PlayerPtr &p)
		{
			auto *character = p->getGameCharacter();
			if (character)
			{
				return (character->getName() == name);
			}

			return false;
		});

		if (p != m_players.end())
		{
			return *p;
		}

		return nullptr;
	}

	void PlayerManager::getCrossRealmGUID(UInt64 & guid)
	{
		const UInt32 lowerPart = guidLowerPart(guid);
		guid = createRealmGUID(
			lowerPart,
			m_realmID,
			guid_type::Player);
	}

}
