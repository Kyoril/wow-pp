#include <functional>
#include <fstream>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/uuid/sha1.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/optional.hpp>
#include <random>
#include <atomic>
#include <forward_list>
#include <initializer_list>
#include <list>
#include <iterator>
#include <exception>
#include <type_traits>
#include "simple/simple.hpp"
#include "common/sha1.h"
#include "project.h"
#include "game/defines.h"

bool wowpp::canStackSpellRanksInSpellBook(const proto::SpellEntry & spell)
{
	// Ranked passive spells don't stack
	if ((spell.attributes(0) & game::spell_attributes::Passive) != 0 &&
		spell.rank() != 0)
		return false;

	// Only mana and health spells allow stacking for efficiency
	if (spell.powertype() != game::power_type::Mana && spell.powertype() != game::power_type::Health)
		return false;

	// TODO: Profession and riding spells don't allow stacking

	// Stances don't allow stacking (We only need to check mana / energy classes here)
	for (const auto &effect : spell.effects())
	{
		switch (spell.family())
		{
			case game::spell_family::Paladin:
				// Paladin auras
				if (effect.type() == game::spell_effects::ApplyAreaAuraParty)
					return false;
				break;
			case game::spell_family::Druid:
			case game::spell_family::Rogue:
				// Druid shapeshifting spells or rogue stealth
				if (effect.type() == game::spell_effects::ApplyAura &&
					effect.aura() == game::aura_type::ModShapeShift)
					return false;
				break;
		}
	}

	return true;
}
