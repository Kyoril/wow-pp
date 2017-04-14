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

#include "common/typedefs.h"
#include "common/countdown.h"

namespace wowpp
{
	// Forward definitions

	class AuraEffect;
	class AuraContainer;
	class GameUnit;
	class TimerQueue;

	namespace proto
	{
		class SpellEntry;
	}

	/// This class manages up to three AuraEffect instances which belong to the same 
	/// spell cast.
	class AuraSpellSlot final : public std::enable_shared_from_this<AuraSpellSlot>
	{
	public:

		/// Value defined by WoW
		static constexpr size_t MaxAuraEffects = 3;

		/// Shared pointer of an AuraEffect instance.
		typedef std::shared_ptr<AuraEffect> AuraEffectPtr;

	private:

		// Make this class non-copyable

		AuraSpellSlot(const AuraSpellSlot &other) = delete;
		AuraSpellSlot &operator=(const AuraSpellSlot &other) = delete;

	public:

		/// Creates a new, empty aura spell slot.
		explicit AuraSpellSlot(TimerQueue &timers, const proto::SpellEntry &spell, UInt64 itemGuid = 0);
		virtual ~AuraSpellSlot();

		/// Applies all aura effects. After calling this method, aura effects may not
		/// be modified before misapplyEffects is called!
		void applyEffects();
		/// Misapplies all aura effects. After calling this method, aura effects may 
		/// be modified again.
		void misapplyEffects();
		/// Applies an aura effect to the list of effects. This method will assert fail
		/// if the effect limit is reached or if the effects have already been applied.
		/// @returns The effect index that has been used.
		UInt16 addAuraEffect(AuraEffectPtr effect);
		/// Updates the owner of this aura. If the effects have been applied before, they
		/// will be misapplied and reapplied again on the new owner (if a valid owner is
		/// provided).
		void setOwner(std::shared_ptr<GameUnit> owner);
		/// Gets the owner of this slot.
		std::shared_ptr<GameUnit> getOwner() const {
			return m_owner;
		}
		/// 
		void setCaster(std::shared_ptr<GameUnit> caster);
		/// 
		GameUnit *getCaster() const;

		/// Determines whether the aura effects have been applied.
		bool hasBeenApplied() const {
			return m_applied;
		}
		/// Gets the spell entry whose cast created this slot.
		const proto::SpellEntry &getSpell() const {
			return m_spell;
		}
		/// Gets the guid of the item that was used to cast the spell which created 
		/// this aura slot instance, or 0 if no item associated.
		UInt64 getItemGuid() const {
			return m_itemGuid;
		}
		/// Determines whether the spell of this aura is a passive spell.
		bool isPassive() const;
		/// Determines whether this is a positive aura.
		bool isPositive() const;
		/// Determines whether this is a negative aura.
		bool isNegative() const;
		/// Determines whether this aura remains active on owners death.
		bool isDeathPersistent() const;
		/// Determines if this aura slot has some mechanics.
		bool hasMechanics(UInt32 mechanicMask) const;
		/// Determines if a specific aura type is added.
		bool hasEffect(UInt32 auraType) const;

		/// Gets the total duration of this aura in milliseconds.
		UInt32 getTotalDuration() const {
			return m_totalDuration;
		}
		/// Determines whether the aura slot is valid.
		bool hasValidSlot() const {
			return m_slot != 0xFF;
		}
		/// Gets the aura's slot.
		UInt8 getSlot() const {
			return m_slot;
		}
		/// Updates the aura's slot.
		void setSlot(UInt8 slot);
		/// Determines whether this aura should override another aura slot.
		bool shouldOverwriteAura(AuraSpellSlot &other);

		/// Executes a function for each effect as long as the functor method returns true.
		void forEachEffect(std::function<bool(AuraEffectPtr)> functor);
		/// Executes a function for each effect of a specific type as long as the functor method returns true.
		void forEachEffectOfType(UInt32 type, std::function<bool(AuraEffectPtr)> functor);

	private:

		/// Executed when the aura slot expires.
		void onExpiration();

	private:

		bool m_applied;
		std::array<AuraEffectPtr, MaxAuraEffects> m_effects;
		std::shared_ptr<GameUnit> m_owner;
		std::weak_ptr<GameUnit> m_caster;
		const proto::SpellEntry &m_spell;
		UInt64 m_itemGuid;
		Int32 m_totalDuration;
		UInt8 m_slot;
		simple::scoped_connection m_onExpire;
		Countdown m_expireCountdown;
	};
}
