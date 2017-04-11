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
#include "player.h"
#include "player_manager.h"
#include "log/default_log_levels.h"
#include "game/world_instance_manager.h"
#include "game/world_instance.h"
#include "game/each_tile_in_sight.h"
#include "game/universe.h"
#include "proto_data/project.h"
#include "game/game_creature.h"
#include "game/game_world_object.h"
#include "game/game_bag.h"
#include "trade_data.h"
#include "game/unit_mover.h"

using namespace std;

namespace wowpp
{
	void Player::handleInitateTrade(game::Protocol::IncomingPacket & packet)
	{
		UInt64 otherGuid;
		if (!(game::client_read::initateTrade(packet, otherGuid)))
		{
			return;
		}

		initiateTrade(otherGuid);
	}

	void Player::handleBeginTrade(game::Protocol::IncomingPacket &packet)
	{
		// Check if we have a trade data
		if (!m_tradeData)
			return;

		// Open the trade windows
		m_tradeData->openWindows();
	}

	void Player::handleAcceptTrade(game::Protocol::IncomingPacket &packet)
	{
		if (!m_tradeData)
			return;

		// Determine trader index
		TradeData::Trader index = 
			(&m_tradeData->getInitiator() == this) ? TradeData::Owner : TradeData::Target;
		m_tradeData->setAcceptedState(index, true);
	}

	void Player::handleUnacceptTrade(game::Protocol::IncomingPacket & packet)
	{
		if (!m_tradeData)
			return;

		// Determine trader index
		TradeData::Trader index =
			(&m_tradeData->getInitiator() == this) ? TradeData::Owner : TradeData::Target;
		m_tradeData->setAcceptedState(index, false);
	}

	void Player::handleCancelTrade(game::Protocol::IncomingPacket & packet)
	{
		// Cancel the current trade (if any)
		cancelTrade();
	}

	void Player::handleSetTradeGold(game::Protocol::IncomingPacket &packet)
	{
		UInt32 gold;
		if (!(game::client_read::setTradeGold(packet, gold)))
		{
			return;
		}

		if (!m_tradeData)
			return;

		TradeData::Trader index =
			(&m_tradeData->getInitiator() == this) ? TradeData::Owner : TradeData::Target;
		m_tradeData->setGold(index, gold);
	}

	void Player::handleSetTradeItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 tradeSlot;
		UInt8 bag;
		UInt8 slot;
		if (!(game::client_read::setTradeItem(packet, tradeSlot, bag, slot)))
			return;

		if (!m_tradeData || !m_character)
		{
			return;
		}

		// Look for item
		auto &inventory = m_character->getInventory();
		auto item = inventory.getItemAtSlot(inventory.getAbsoluteSlot(bag, slot));
		if (!item)
		{
			WLOG("Item not found");
			return;
		}

		// Set item
		TradeData::Trader index =
			(&m_tradeData->getInitiator() == this) ? TradeData::Owner : TradeData::Target;
		m_tradeData->setItem(index, tradeSlot, item);
	}

	void Player::handleClearTradeItem(game::Protocol::IncomingPacket & packet)
	{
		UInt8 slot;
		if (!(game::client_read::clearTradeItem(packet, slot)))
			return;

		if (!m_tradeData || !m_character)
		{
			return;
		}

		// Set item
		TradeData::Trader index =
			(&m_tradeData->getInitiator() == this) ? TradeData::Owner : TradeData::Target;
		m_tradeData->setItem(index, slot, nullptr);
	}
}
