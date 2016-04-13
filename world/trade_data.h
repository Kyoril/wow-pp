#pragma once

#include "common/typedefs.h"

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
		void setItem(UInt64 item_guid, int index);

	
	private:
		Player *m_player;
		Player *m_trader;
		UInt32 gold;
		bool acceptTrade;
		UInt64 m_item_guid[7];
	};
}
