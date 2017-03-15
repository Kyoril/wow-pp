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
#include "trade_data.h"
#include "game/game_creature.h"
#include "game_protocol/game_protocol.h"
#include "player.h"
#include "game/game_world_object.h"


namespace wowpp
{
	constexpr size_t TradeData::MaxTradeSlots;
	
	TradeData::TradeData(Player &player, Player &trader)
		: m_initiator(player)
		, m_other(trader)
	{
	}

	void TradeData::openWindows()
	{
		m_initiator.sendTradeStatus(game::trade_status::OpenWindow);
		m_other.sendTradeStatus(game::trade_status::OpenWindow);
	}

	void TradeData::cancel()
	{
		m_initiator.sendTradeStatus(game::trade_status::TradeCanceled);
		m_other.sendTradeStatus(game::trade_status::TradeCanceled);

		// Resetting the trade session field for both player instances will destroy this
		// instance
		destroy();
	}

	void TradeData::performTrade()
	{
		// Get both characters
		auto ownerChar = m_initiator.getCharacter();
		ASSERT(ownerChar);
		auto otherChar = m_other.getCharacter();
		ASSERT(otherChar);

		const UInt32 ownerMoney = ownerChar->getUInt32Value(character_fields::Coinage);
		const UInt32 otherMoney = otherChar->getUInt32Value(character_fields::Coinage);

		// Perform some checks, some of them again
		{
			// Check owner gold
			if (ownerMoney < m_data[Owner].gold)
			{
				sendTradeError(game::inventory_change_failure::NotEnoughMoney);
				return;
			}

			// Check other gold
			if (otherMoney < m_data[Target].gold)
			{
				sendTradeError(game::inventory_change_failure::NotEnoughMoney);
				return;
			}
		}

		// Used for inventory checks
		auto &ownerInv = ownerChar->getInventory();
		auto &otherInv = otherChar->getInventory();

		// Calculate the amount of inventory slots that are required for both players
		const UInt32 targetItemCount = getTradedItemCount(Target);
		const UInt32 ownerItemCount = getTradedItemCount(Owner);
		const Int32 requiredOwnerSlots = targetItemCount - ownerItemCount;
		const Int32 requiredTargetSlots = ownerItemCount - targetItemCount;

		// Check if both players have enough space in their inventory
		if (ownerInv.getFreeSlotCount() < requiredOwnerSlots)
		{
			WLOG("Not enough space in initiators inventory.");
			sendTradeError(game::inventory_change_failure::InventoryFull);
			return;
		}
		if (otherInv.getFreeSlotCount() < requiredTargetSlots)
		{
			WLOG("Not enough space in targets inventory.");
			sendTradeError(game::inventory_change_failure::InventoryFull);
			return;
		}

		// Now remove all items from the inventory first - don't worry: They won't be deleted from memory
		// since we use shared_ptrs which are also stored in the m_data field. We remove these items from
		// the inventory first, so that there is enough space in each inventory in case it is needed (overflow
		// was checked before, so it is safe here)

		removeItems(Owner, ownerInv);
		removeItems(Target, otherInv);

		// Now add items to the others inventory
		addItems(Owner, otherInv);
		addItems(Target, ownerInv);

		// Perform gold trade
		{
			// Increment gold values
			ownerChar->setUInt32Value(character_fields::Coinage, ownerMoney - m_data[Owner].gold + m_data[Target].gold);
			otherChar->setUInt32Value(character_fields::Coinage, otherMoney - m_data[Target].gold + m_data[Owner].gold);
		}

		// Finalize trade, which will close the trade window on both clients
		m_initiator.sendTradeStatus(game::trade_status::TradeComplete);
		m_other.sendTradeStatus(game::trade_status::TradeComplete);

		// Destroy this trade session
		destroy();
	}

	UInt32 TradeData::getTradedItemCount(Trader index) const
	{
		UInt32 count = MaxTradeSlots - 1;
		for (const auto &item : m_data[index].items)
		{
			if (!item)
			{
				count--;
			}
		}
		return count;
	}

	void TradeData::setGold(Trader index, UInt32 gold)
	{
		ASSERT(index > Trader::Count_);

		// Update gold value and acceptance state
		m_data[index].gold = gold;

		// Send data
		sendTradeData(index);

		// Refresh trade state
		setAcceptedState(Owner, false);
		setAcceptedState(Target, false);
	}

	void TradeData::setAcceptedState(Trader index, bool accept)
	{
		ASSERT(index > Trader::Count_);

		bool wasAccepted = m_data[index].accepted;
		if (accept == wasAccepted)
			return;

		m_data[index].accepted = accept;

		// If no more accepted, send back to trade
		if (wasAccepted)
		{
			// Notify both players about the cancellation
			m_initiator.sendTradeStatus(game::trade_status::BackToTrade);
			m_other.sendTradeStatus(game::trade_status::BackToTrade);
			return;
		}
		else
		{
			// Did both accept?
			if (m_data[Owner].accepted && m_data[Target].accepted)
			{
				performTrade();
			}
			else
			{
				Player *target = (index == Owner ? &m_other : &m_initiator);
				ASSERT(target);

				// Notify the other client about the acceptance
				target->sendTradeStatus(game::trade_status::TradeAccept);
			}
		}
	}

	void TradeData::setItem(Trader index, UInt8 tradeSlot, ItemPtr item)
	{
		ASSERT(index < Trader::Count_);
		ASSERT(tradeSlot < MaxTradeSlots);

		// First remove that item from any other trade slot where it eventually is
		if (item != nullptr)
		{
			for (auto &oldItem : m_data[index].items)
			{
				if (oldItem && oldItem->getGuid() == item->getGuid())
				{
					oldItem = nullptr;
				}
			}
		}
		
		// Now set the new one
		m_data[index].items[tradeSlot] = item;

		// Send new items to other client
		sendTradeData(index);

		// Reset trade state
		setAcceptedState(Owner, false);
		setAcceptedState(Target, false);
	}
	void TradeData::sendTradeData(Trader index)
	{
		// Send data
		auto formatter = std::bind(game::server_write::sendUpdateTrade, std::placeholders::_1,
			1,
			0,
			MaxTradeSlots,
			MaxTradeSlots,
			m_data[index].gold,
			0,
			std::cref(m_data[index].items));
		if (index == Owner)
		{
			m_other.sendProxyPacket(formatter);
		}
		else
		{
			m_initiator.sendProxyPacket(formatter);
		}
	}
	void TradeData::sendTradeError(UInt32 errorCode, UInt32 itemCategory)
	{
		// Send new items to other client
		m_initiator.sendTradeStatus(game::trade_status::CloseWindow, 0, errorCode, itemCategory);
		m_other.sendTradeStatus(game::trade_status::CloseWindow, 0, errorCode, itemCategory);

		destroy();
	}

	void TradeData::destroy()
	{
		// Destroy trade data
		m_initiator.setTradeSession(nullptr);
		m_other.setTradeSession(nullptr);
	}

	void TradeData::removeItems(Trader index, Inventory & inventory)
	{
		for (const auto &item : m_data[index].items)
		{
			if (item)
			{
				if (inventory.removeItemByGUID(item->getGuid()) != game::inventory_change_failure::Okay)
				{
					ELOG("Could not remove item from inventory!");
					ASSERT(false);
					return;
				}
			}
		}
	}
	void TradeData::addItems(Trader index, Inventory & inventory)
	{
		for (auto item : m_data[index].items)
		{
			if (item)
			{
				if (inventory.addItem(item) != game::inventory_change_failure::Okay)
				{
					ELOG("Could not add item to inventory!");
					ASSERT(false);
					return;
				}
			}
		}
	}
}
