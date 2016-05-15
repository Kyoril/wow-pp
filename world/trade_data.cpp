#include "pch.h"
#include "trade_data.h"
#include "game/game_creature.h"
#include "Player.h"
#include "game/game_world_object.h"


namespace wowpp
{
	TradeData::TradeData() {}
	TradeData::TradeData(Player *_player, Player *_trader) : m_player(_player), m_trader(_trader), gold(0) {}

	Player* TradeData::getPlayer()
	{
		return m_player;
	}

	Player* TradeData::getTrader()
	{
		return m_trader;
	}

	void TradeData::setGold(UInt32 money)
	{
		gold = money;
	}

	void TradeData::setacceptTrade(bool state)
	{
		acceptTrade = state;
	}

	UInt32 TradeData::getGold()
	{
		return gold;
	}

	bool TradeData::isAccepted()
	{
		return acceptTrade;
	}

	void TradeData::setItem(proto::ItemEntry item, UInt16 slot)
	{
		int _slot = slot + 1;
		items.insert(items.begin() + _slot, item);
	}
	std::vector<proto::ItemEntry> TradeData::getItem()
	{
		return(items);
	}
}
