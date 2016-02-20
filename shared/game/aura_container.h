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

#pragma once

#include "aura.h"
#include <boost/optional.hpp>

namespace wowpp
{
	class GameUnit;

	/// Holds and manages instances of auras for one unit.
	class AuraContainer final
	{
	private:

		typedef std::list<std::shared_ptr<Aura>> AuraList;

	public:

		explicit AuraContainer(GameUnit &owner);

		bool addAura(std::shared_ptr<Aura> aura);
		void removeAura(AuraList::iterator &it);
		void removeAura(Aura &aura);
		void handleTargetDeath();
		void removeAllAurasDueToSpell(UInt32 spellId);
		void removeAurasByType(UInt32 auraType);
		Aura *popBack(UInt8 dispelType, bool positive);
		void removeAllAurasDueToInterrupt(game::SpellAuraInterruptFlags flags);

		GameUnit &getOwner() {
			return m_owner;
		}
		size_t getSize() const {
			return m_auras.size();
		}
		bool hasAura(game::AuraType type) const;
		UInt32 consumeAbsorb(UInt32 damage, UInt8 school);
		Int32 getMaximumBasePoints(game::AuraType type) const;
		Int32 getMinimumBasePoints(game::AuraType type) const;
		Int32 getTotalBasePoints(game::AuraType type) const;
		float getTotalMultiplier(game::AuraType type) const;

	private:

		GameUnit &m_owner;
		AuraList m_auras;

		AuraList::iterator findAura(Aura &aura);

	};

}
