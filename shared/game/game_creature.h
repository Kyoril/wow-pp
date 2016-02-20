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
#include "world_instance.h"
#include "unit_watcher.h"
#include "game_character.h"
#include "loot_instance.h"
#include "common/linear_set.h"
#include <boost/signals2.hpp>

namespace wowpp
{
	class CreatureAI;
	namespace proto
	{
		class UnitEntry;
		class ItemEntry;
	}

	/// Represents an AI controlled creature unit in the game.
	class GameCreature final : public GameUnit
	{
		typedef LinearSet<UInt64> LootRecipients;

	public:

		/// Executed when the unit entry was changed after this creature has spawned. This
		/// can happen if the unit transforms.
		boost::signals2::signal<void()> entryChanged;

	public:

		/// Creates a new instance of the GameCreature class.
		explicit GameCreature(
		    proto::Project &project,
		    TimerQueue &timers,
		    const proto::UnitEntry &entry);

		/// @copydoc GameObject::initialize()
		virtual void initialize() override;
		/// @copydoc GameObject::getTypeId()
		virtual ObjectType getTypeId() const override {
			return object_type::Unit;
		}
		/// Gets the original unit entry (the one, this creature was spawned with).
		/// This is useful for restoring the original creature state.
		const proto::UnitEntry &getOriginalEntry() const {
			return m_originalEntry;
		}
		/// Gets the unit entry on which base this creature has been created.
		const proto::UnitEntry &getEntry() const {
			return *m_entry;
		}
		/// Changes the creatures entry index. Remember, that the creature always has to
		/// have a valid base entry.
		void setEntry(const proto::UnitEntry &entry);
		///
		const String &getName() const override;
		///
		void setVirtualItem(UInt32 slot, const proto::ItemEntry *item);
		///
		bool canBlock() const override;
		///
		bool canParry() const override;
		///
		bool canDodge() const override;
		///
		bool canDualWield() const override;
		///
		void updateDamage() override;
		/// Updates the creatures loot recipient. Values of 0 mean no recipient.
		void addLootRecipient(UInt64 guid);
		/// Removes all loot recipients.
		void removeLootRecipients();
		/// Determines whether a specific character is allowed to loot this creature.
		bool isLootRecipient(GameCharacter &character) const;
		/// Determines whether this creature is tagged by a player or group.
		bool isTagged() const {
			return !m_lootRecipients.empty();
		}
		/// Get unit loot.
		LootInstance *getUnitLoot() {
			return m_unitLoot.get();
		}
		void setUnitLoot(std::unique_ptr<LootInstance> unitLoot);
		/// Gets the number of loot recipients.
		UInt32 getLootRecipientCount() const {
			return m_lootRecipients.size();
		}
		///
		void raiseTrigger(trigger_event::Type e);
		/// @copydoc GameObject::providesQuest()
		bool providesQuest(UInt32 questId) const override;
		/// @copydoc GameObject::endsQuest()
		bool endsQuest(UInt32 questId) const override;
		game::QuestgiverStatus getQuestgiverStatus(const GameCharacter &character) const;

		bool hasMainHandWeapon() const override;
		bool hasOffHandWeapon() const override;

		/// Executes a callback function for every valid loot recipient.
		template<typename OnRecipient>
		void forEachLootRecipient(OnRecipient callback)
		{
			if (!getWorldInstance()) {
				return;
			}

			for (auto &guid : m_lootRecipients)
			{
				GameCharacter *character = dynamic_cast<GameCharacter *>(getWorldInstance()->findObjectByGUID(guid));
				if (character)
				{
					callback(*character);
				}
			}
		}

	protected:

		///
		virtual void regenerateHealth() override;

	private:

		float calcXpModifier(UInt32 attackerLevel) const;

	private:

		const proto::UnitEntry &m_originalEntry;
		const proto::UnitEntry *m_entry;
		std::unique_ptr<CreatureAI> m_ai;
		boost::signals2::scoped_connection m_onSpawned;
		LootRecipients m_lootRecipients;
		std::unique_ptr<LootInstance> m_unitLoot;
	};

	UInt32 getZeroDiffXPValue(UInt32 killerLevel);
	UInt32 getGrayLevel(UInt32 killerLevel);
}
