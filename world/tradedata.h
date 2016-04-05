#pragma once

//#include "shared/game/game_character.h"

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
	
	private:
		Player *m_player;
		Player *m_trader;
	};
}