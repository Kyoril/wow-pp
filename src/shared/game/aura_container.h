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

#include "game/defines.h"

namespace wowpp
{
	class GameUnit;
	class AuraEffect;
	class AuraSpellSlot;

	/// This structure is used to send and receive aura data between server nodes and the 
	/// realm server.
	struct AuraData final
	{
		UInt64 casterGuid;
		UInt64 itemGuid;
		UInt32 spell;
		UInt32 stackCount;
		UInt32 remainingCharges;
		std::array<Int32, 3> basePoints;
		std::array<Int32, 3> periodicTime;
		UInt32 maxDuration;
		UInt32 remainingTime;
		UInt32 effectIndexMask;

		AuraData()
			: casterGuid(0)
			, itemGuid(0)
			, spell(0)
			, stackCount(0)
			, remainingCharges(0)
			, maxDuration(0)
			, remainingTime(0)
			, effectIndexMask(0)
		{
		}
	};


	/// Holds and manages instances of auras for one unit.
	class AuraContainer final
	{
	private:

		typedef std::shared_ptr<AuraSpellSlot> AuraPtr;
		typedef simple::stable_list<AuraPtr> AuraList;

	public:

		/// Initializes a new AuraContainer for a specific owner unit.
		explicit AuraContainer(GameUnit &owner);

		/// Adds a new aura to the list of active auras.
		bool addAura(AuraPtr aura, bool restoration = false);
		/// Removes a specific auras.
		void removeAura(AuraList::iterator &it);
		/// Removes a specific aura slot instance.
		void removeAura(AuraSpellSlot &aura);
		/// Called when the owner dies and will remove all aura slots which aren't 
		/// marked death persistent.
		void handleTargetDeath();
		/// Removes all auras of a specific spell id.
		void removeAllAurasDueToSpell(UInt32 spellId);
		/// Removes all auras caused by a specific item instance.
		void removeAllAurasDueToItem(UInt64 itemGuid);
		/// Removes all auras which apply a specific mechanic.
		void removeAllAurasDueToMechanic(UInt32 immunityMask);
		/// Removes a certain amount of auras which match a specific dispel type 
		/// (Magic, Curse, Poison etc.)
		UInt32 removeAurasDueToDispel(UInt32 dispelType, bool dispelPositive, UInt32 count = 1);
		/// Removes all aura slots which own a specific aura effect type.
		void removeAurasByType(UInt32 auraType);
		/// Removes all auras which are marked to be removed by specific interrupt flags.
		void removeAllAurasDueToInterrupt(game::SpellAuraInterruptFlags flags);
		/// Removes all auras.
		void removeAllAuras();
		/// Outputs debug infos about all auras in this container.
		void logAuraInfos();
		/// Serializes all auras to the given aura data vector.
		bool serializeAuraData(std::vector<AuraData>& out_data) const;
		/// Restores the aura container from the given aura data.
		bool restoreAuraData(const std::vector<AuraData>& data);

		/// Gets a reference on the owning unit.
		GameUnit &getOwner() {
			return m_owner;
		}
		/// Determines if at least one aura with a certain aura effect is active.
		bool hasAura(game::AuraType type) const;
		/// Consumes absorb effects from as many auras as possible, one after another.
		UInt32 consumeAbsorb(UInt32 damage, UInt8 school);
		/// 
		Int32 getMaximumBasePoints(game::AuraType type) const;
		/// 
		Int32 getMinimumBasePoints(game::AuraType type) const;
		/// 
		Int32 getTotalBasePoints(game::AuraType type) const;
		/// 
		float getTotalMultiplier(game::AuraType type) const;

		/// Executes a function callback for every aura effect.
		void forEachAura(std::function<bool(AuraEffect &)> functor);
		/// Executes a function callback for every aura effect of a specific type.
		void forEachAuraOfType(game::AuraType type, std::function<bool(AuraEffect &)> functor);

	private:

		/// Gets an iteratore of a specific aura slot.
		AuraList::iterator findAura(AuraSpellSlot &aura);

	private:

		GameUnit &m_owner;
		AuraList m_auras;
		std::map<UInt16, UInt16> m_auraTypeCount;
	};

}
