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

#include "game_unit.h"
#include "data/unit_entry.h"
#include "unit_watcher.h"
#include <boost/signals2.hpp>

namespace wowpp
{
	class CreatureAI;

	/// Represents an AI controlled creature unit in the game.
	class GameCreature final : public GameUnit
	{
	public:

		/// Executed when the unit entry was changed after this creature has spawned. This
		/// can happen if the unit transforms.
		boost::signals2::signal<void()> entryChanged;

	public:

		/// Creates a new instance of the GameCreature class.
		explicit GameCreature(
			TimerQueue &timers,
			DataLoadContext::GetRace getRace,
			DataLoadContext::GetClass getClass,
			DataLoadContext::GetLevel getLevel,
			DataLoadContext::GetSpell getSpell,
			const UnitEntry &entry);

		/// @copydoc GameObject::initialize()
		virtual void initialize() override;
		/// @copydoc GameObject::getTypeId()
		virtual ObjectType getTypeId() const override { return object_type::Unit; }
		/// Gets the original unit entry (the one, this creature was spawned with).
		/// This is useful for restoring the original creature state.
		const UnitEntry &getOriginalEntry() const { return m_originalEntry; }
		/// Gets the unit entry on which base this creature has been created.
		const UnitEntry &getEntry() const { return *m_entry; }
		/// Changes the creatures entry index. Remember, that the creature always has to
		/// have a valid base entry.
		void setEntry(const UnitEntry &entry);
		/// 
		const String &getName() const override;
		/// 
		void setVirtualItem(UInt32 slot, const ItemEntry *item);
		/// 
		bool canBlock() const override;
		/// 
		bool canParry() const override;
		/// 
		bool canDodge() const override;
		/// 
		void updateDamage() override;

	protected:

		/// 
		void onKilled(GameUnit *killer) override;
		virtual void regenerateHealth() override;

	private:

		float calcXpModifier(UInt32 attackerLevel) const;

	private:

		const UnitEntry &m_originalEntry;
		const UnitEntry *m_entry;
		//ThreatList m_threat;
		std::unique_ptr<CreatureAI> m_ai;
	};

	UInt32 getZeroDiffXPValue(UInt32 killerLevel);
	UInt32 getGrayLevel(UInt32 killerLevel);
}
