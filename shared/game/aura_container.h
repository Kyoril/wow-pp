//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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
	public:

		explicit AuraContainer(GameUnit &owner);

		bool addAura(std::shared_ptr<Aura> aura);
		size_t findAura(Aura &aura, size_t begin);
		void removeAura(size_t index);
		Aura &get(size_t index);
		const Aura &get(size_t index) const;
		void handleTargetDeath();
		void removeAllAurasDueToSpell(UInt32 spellId);

		GameUnit &getOwner() { return m_owner; }
		size_t getSize() const { return m_auras.size(); }
		bool hasAura(game::AuraType type) const;

	private:

		typedef std::vector<std::shared_ptr<Aura>> AuraVector;

		GameUnit &m_owner;
		AuraVector m_auras;
	};

	boost::optional<std::size_t> findAuraInstanceIndex(
		AuraContainer &instances,
		Aura &instance);
}
