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

#include "player.h"
#include "player_manager.h"
#include "log/default_log_levels.h"
#include "world_instance_manager.h"
#include "world_instance.h"
#include <cassert>
#include <limits>

using namespace std;

namespace wowpp
{
	Player::Player(PlayerManager &manager, RealmConnector &realmConnector, WorldInstanceManager &worldInstanceManager, DatabaseId characterId, std::shared_ptr<GameCharacter> character, WorldInstance &instance)
		: m_manager(manager)
		, m_realmConnector(realmConnector)
		, m_worldInstanceManager(worldInstanceManager)
		, m_characterId(characterId)
		, m_character(std::move(character))
		, m_logoutCountdown(worldInstanceManager.getTimerQueue())
		, m_instance(instance)
	{
		m_logoutCountdown.ended.connect(
			std::bind(&Player::onLogout, this));
	}

	void Player::logoutRequest()
	{
		// Make our character sit down
		auto standState = unit_stand_state::Sit;
		m_character->setByteValue(unit_fields::Bytes1, 0, standState);
		sendProxyPacket(
			std::bind(game::server_write::standStateUpdate, std::placeholders::_1, standState));

		// Setup the logout countdown
		m_logoutCountdown.setEnd(
			getCurrentTime() + (20 * constants::OneSecond));
	}

	void Player::cancelLogoutRequest()
	{
		// Stand up again
		auto standState = unit_stand_state::Stand;
		m_character->setByteValue(unit_fields::Bytes1, 0, standState);
		sendProxyPacket(
			std::bind(game::server_write::standStateUpdate, std::placeholders::_1, standState));

		// Cancel the countdown
		m_logoutCountdown.cancel();
	}

	void Player::onLogout()
	{
		// Remove the character from the world
		m_instance.removeGameObject(*m_character);
		m_character.reset();

		// Notify the realm
		m_realmConnector.notifyWorldInstanceLeft(m_characterId, pp::world_realm::world_left_reason::Logout);
		
		// Remove player
		m_manager.playerDisconnected(*this);
	}

	void Player::chatMessage(game::ChatMsg type, game::Language lang, const String &receiver, const String &channel, const String &message)
	{
		if (!m_character)
		{
			WLOG("No character assigned!");
			return;
		}

		float x, y, z, o;
		m_character->getLocation(x, y, z, o);

		// Get a list of potential watchers
		auto &grid = m_instance.getGrid();
		
		// Get tile index
		TileIndex2D tile;
		grid.getTilePosition(x, y, z, tile[0], tile[1]);

		// Get all potential characters
		std::vector<Player*> subscribers;
		forEachTileInSight(
			grid,
			tile,
			[&subscribers](VisibilityTile &tile)
		{
			for (auto * const subscriber : tile.getWatchers().getElements())
			{
				subscribers.push_back(subscriber);
			}
		});

		switch (type)
		{
			case game::chat_msg::Say:
			{
				// Send to all subscribers in range
				for (auto * const subscriber : subscribers)
				{
					const float distance = m_character->getDistanceTo(*subscriber->getCharacter());
					if (distance <= 25.0f)	// 25 yards range
					{
						subscriber->sendProxyPacket(
							std::bind(game::server_write::messageChat, std::placeholders::_1, type, lang, std::cref(channel), m_character->getGuid(), std::cref(message), m_character.get()));
					}
				}

				break;
			}

			case game::chat_msg::Yell:
			{
				// Send to all subscribers in range
				for (auto * const subscriber : subscribers)
				{
					const float distance = m_character->getDistanceTo(*subscriber->getCharacter());
					if (distance <= 300.0f)	// 300 yards range
					{
						subscriber->sendProxyPacket(
							std::bind(game::server_write::messageChat, std::placeholders::_1, type, lang, std::cref(channel), m_character->getGuid(), std::cref(message), m_character.get()));
					}
				}

				break;
			}

			default:
			{
				WLOG("Chat mode unimplemented");
				break;
			}
		}
	}

}
