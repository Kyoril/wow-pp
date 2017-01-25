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
		TradeStatusInfo statusInfo;
		if (!(game::client_read::initateTrade(packet, otherGuid)))
		{
			return;
		}

		if (!m_character)
		{
			return;
		}

		// Can't trade while dead
		if (!m_character->isAlive())
		{
			sendTradeStatus(game::trade_status::YouDead);
			return;
		}

		if (m_character->isStunned())
		{
			sendTradeStatus(game::trade_status::YouStunned);
			return;
		}

		if (m_logoutCountdown.running)
		{
			sendTradeStatus(game::trade_status::YouLogout);
			return;
		}

		auto *worldInstance = m_character->getWorldInstance();
		if (!worldInstance)
		{
			return;
		}

		UInt64 thisguid = m_character->getGuid();

		auto *otherPlayer = m_manager.getPlayerByCharacterGuid(otherGuid);
		if (otherPlayer == nullptr ||
			!otherPlayer->getCharacter())
		{
			sendTradeStatus(game::trade_status::NoTarget);
			return;
		}

		auto otherCharacter = otherPlayer->getCharacter();
		if (!otherCharacter->isAlive())
		{
			sendTradeStatus(game::trade_status::TargetDead);
			return;
		}

		if (otherCharacter->isStunned())
		{
			sendTradeStatus(game::trade_status::TargetStunned);
			return;
		}

		// TODO: Target logout and other checks

		if (otherCharacter->getDistanceTo(*m_character, false) > 10.0f)
		{
			sendTradeStatus(game::trade_status::TargetTooFar);
			return;
		}

		// Start trade
		m_tradeStatusInfo.tradestatus = game::trade_status::BeginTrade;
		m_tradeStatusInfo.guid = thisguid;
		m_tradeData = std::make_shared<TradeData>(this, otherPlayer);
		otherPlayer->m_tradeData = std::make_shared<TradeData>(otherPlayer, this);
		otherPlayer->sendTradeStatus(m_tradeStatusInfo);
	}

	void Player::handleBeginTrade(game::Protocol::IncomingPacket &packet)
	{
		if (!m_tradeData)
		{
			return;
		}

		m_tradeStatusInfo.tradestatus = game::trade_status::OpenWindow;

		m_tradeData->getPlayer()->sendTradeStatus(m_tradeStatusInfo);
		m_tradeData->getTrader()->sendTradeStatus(m_tradeStatusInfo);

		//openWindow
	}

	void Player::handleAcceptTrade(game::Protocol::IncomingPacket &packet)
	{
		std::shared_ptr<TradeData> my_Trade = m_tradeData;
		if (nullptr == my_Trade)
		{
			return;
		}

		std::shared_ptr<GameItem> playerItems[trade_slots::TradedCount];
		std::shared_ptr<GameItem> traderItems[trade_slots::TradedCount];

		Player* trader = my_Trade->getTrader();
		Player* player = my_Trade->getPlayer();
		std::shared_ptr<TradeData> his_Trade = trader->m_tradeData;
		if (nullptr == his_Trade)
		{
			return;
		}

		my_Trade->setTradeAcceptState(true);

		TradeStatusInfo info;

		if (my_Trade->getGold() > player->getCharacter()->getUInt32Value(character_fields::Coinage))
		{
			info.tradestatus = game::trade_status::CloseWindow;
			sendTradeStatus(info);
			my_Trade->setTradeAcceptState(false);
			return;
		}

		if (his_Trade->getGold() > trader->getCharacter()->getUInt32Value(character_fields::Coinage))
		{
			info.tradestatus = game::trade_status::CloseWindow;
			sendTradeStatus(info);
			his_Trade->setTradeAcceptState(false);
			return;
		}

		//anti cheat for items



		if (his_Trade->isAccepted())
		{
			//check for cheating
			//inform partner client

			info.tradestatus = game::trade_status::TradeAccept;
			trader->sendTradeStatus(info);
			//test item << inventory

			TradeStatusInfo playerCanComplete, traderCanComplete;

			//TODO: checkt if there is enough place i inventory


			std::vector<std::shared_ptr<GameItem>> my_Items;
			my_Items = my_Trade->getItem();

			for (int i = 0; TradeSlots::Count < 1; i++)
			{
				if (my_Items[i] != nullptr)
				{
					playerItems[i] = my_Items[i];
				}
			}
			std::vector<std::shared_ptr<GameItem>> his_Items;
			his_Items = his_Trade->getItem();

			for (int i = 0; i < TradeSlots::Count; i++)
			{
				if (his_Items[i] != nullptr)
				{
					traderItems[i] = his_Items[i];
				}
			}

			moveItems(my_Items, his_Items);



			//execute Trade

			//update money

			UInt32 gold_nowp = player->getCharacter()->getUInt32Value(character_fields::Coinage);
			UInt32 gold_newp = gold_nowp - my_Trade->getGold();

			gold_newp += trader->m_tradeData->getGold();
			player->getCharacter()->setUInt32Value(character_fields::Coinage, gold_newp);


			UInt32 gold_nowt = trader->getCharacter()->getUInt32Value(character_fields::Coinage);
			UInt32 gold_newt = gold_nowt - trader->m_tradeData->getGold();

			gold_newt += my_Trade->getGold();
			trader->getCharacter()->setUInt32Value(character_fields::Coinage, gold_newt);

			info.tradestatus = game::trade_status::TradeComplete;
			trader->sendTradeStatus(info);
			sendTradeStatus(info);
		}
		else
		{
			info.tradestatus = game::trade_status::TradeAccept;
			trader->sendTradeStatus(info);
		}
	}

	void Player::handleSetTradeGold(game::Protocol::IncomingPacket &packet)
	{
		UInt32 gold;
		if (!(game::client_read::setTradeGold(packet, gold)))
		{
			return;
		}

		if (!m_tradeData)
		{
			return;
		}

		if (gold != m_tradeData->getGold())
		{
			// Update the amount of gold
			m_tradeData->setGold(gold);

			// Update trade status
			if (m_tradeData->isAccepted())
			{
				m_tradeData->setTradeAcceptState(false);

				TradeStatusInfo info;
				info.tradestatus = game::trade_status::BackToTrade;
				m_tradeData->getPlayer()->sendTradeStatus(std::move(info));
			}

			// 
			m_tradeData->getTrader()->sendUpdateTrade();
		}


	}

	void Player::handleSetTradeItem(game::Protocol::IncomingPacket &packet)
	{
		UInt8 tradeSlot;
		UInt8 bag;
		UInt8 slot;
		if (!(game::client_read::setTradeItem(packet, tradeSlot, bag, slot)))
		{
			return;
		}

		std::shared_ptr<TradeData> my_Trade = m_tradeData;
		if (my_Trade == nullptr)
		{
			return;
		}

		TradeStatusInfo info;

		if (tradeSlot >= trade_slots::Count)
		{
			info.tradestatus = game::trade_status::TradeCanceled;
			sendTradeStatus(info);
			return;
		}

		UInt16 absSlot = Inventory::getAbsoluteSlot(bag, slot);
		auto this_player = this->getCharacter();
		auto &inventory = this_player->getInventory();
		auto item = inventory.getItemAtSlot(absSlot);
		//TODO: ask if there is an item like that in trade already
		if (item == nullptr)
		{
			DLOG("Item is nullptr");
		}
		my_Trade->setTradeAcceptState(false);
		my_Trade->getTrader()->m_tradeData->setTradeAcceptState(false);


		info.tradestatus = game::trade_status::BackToTrade;
		m_tradeData->getPlayer()->sendTradeStatus(std::move(info));
		my_Trade->setAbsSlot(absSlot, tradeSlot);
		my_Trade->setItem(item, tradeSlot);
		m_tradeData->getTrader()->sendUpdateTrade();
	}
}
