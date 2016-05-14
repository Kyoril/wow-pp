#pragma once

#include "common/typedefs.h"
#include "proto_data/project.h"

namespace wowpp
{
	class Player;
	class TradeData
	{
	public:
		TradeData();
		TradeData(Player *_player, Player *_trader); //trader = other, player = this
		
		Player* getPlayer();
		Player* getTrader();
		void setGold(UInt32 money);
		UInt32 getGold();
		void setacceptTrade(bool state);
		bool isAccepted();
		void setItem(proto::ItemEntry item, UInt16 slot);
		std::vector<proto::ItemEntry> getItem();
	
	private:
		Player *m_player;
		Player *m_trader;
		UInt32 gold;
		bool acceptTrade;
		std::vector<proto::ItemEntry> items;
	};
}
