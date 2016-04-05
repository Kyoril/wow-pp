#include "tradedata.h"
#include "pch.h"
#include "game/game_creature.h"
#include "Player.h"
#include "game/game_world_object.h"

namespace wowpp
{
	TradeData::TradeData() {}
	TradeData::TradeData(Player *_player, Player *_trader): m_player(_player), m_trader(_trader){}
	
	Player* TradeData::getPlayer()
	{
		return m_player;
	}
	
	Player* TradeData::getTrader()
	{
		return m_trader;
	}
}