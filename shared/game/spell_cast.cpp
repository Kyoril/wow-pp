
#include "spell_cast.h"
#include "game_unit.h"
#include "common/countdown.h"

namespace wowpp
{
	SpellCast::SpellCast(GameUnit &executer)
		: m_executer(executer)
	{
	}
}
