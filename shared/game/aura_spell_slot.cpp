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

#include "pch.h"
#include "aura_spell_slot.h"
#include "aura_effect.h"
#include "common/macros.h"
#include "shared/proto_data/spells.pb.h"
#include "common/timer_queue.h"
#include "game_unit.h"

namespace wowpp
{
	AuraSpellSlot::AuraSpellSlot(TimerQueue &timers, const proto::SpellEntry &spell, UInt64 itemGuid/* = 0*/)
		: m_applied(false)
		, m_spell(spell)
		, m_itemGuid(itemGuid)
		, m_totalDuration(0)
		, m_slot(0xFF)
		, m_expireCountdown(timers)
		, m_procCharges(spell.proccharges())
		, m_stackCount(1)
	{
		m_expireCountdown.ended.connect(this, &AuraSpellSlot::onExpiration);
	}

	AuraSpellSlot::~AuraSpellSlot()
	{
		ASSERT(!m_applied && "AuraSpellSlot instance destroyed without being misapplied before");
	}

	void AuraSpellSlot::applyEffects()
	{
		ASSERT(!m_applied && "Aura effects already applied");
		ASSERT(m_owner && "Valid owner required");

		// Apply every single effect
		for (auto effect : m_effects)
		{
			if (effect)
				effect->applyAura();
			else
				break;
		}

		// Set expiration countdown (if any)
		if (m_totalDuration > 0) {
			m_expireCountdown.setEnd(getCurrentTime() + m_totalDuration);
		}

		m_applied = true;

		if (hasValidSlot())
		{
			// Update owner fields
			auto *caster = getCaster();

			m_owner->setUInt32Value(unit_fields::AuraEffect + m_slot, m_spell.id());
			UInt32 index = m_slot / 4;
			UInt32 byte = (m_slot % 4) * 8;
			UInt32 val = m_owner->getUInt32Value(unit_fields::AuraLevels + index);
			val &= ~(0xFF << byte);
			val |= ((caster ? caster->getLevel() : 70) << byte);
			m_owner->setUInt32Value(unit_fields::AuraLevels + index, val);
			val = m_owner->getUInt32Value(unit_fields::AuraFlags + index);
			val &= ~(0xFF << byte);
			if (isPositive()) {
				val |= (UInt32(31) << byte);
			}
			else {
				val |= (UInt32(9) << byte);
			}
			m_owner->setUInt32Value(unit_fields::AuraFlags + index, val);

			// Update aura applications
			updateAuraApplication();

			// Notify caster
			m_owner->auraUpdated(m_slot, m_spell.id(), getTotalDuration(), getTotalDuration());
			if (caster)
			{
				caster->targetAuraUpdated(m_owner->getGuid(), m_slot,
					m_spell.id(), getTotalDuration(), getTotalDuration());
			}
		}
	}

	void AuraSpellSlot::misapplyEffects()
	{
		ASSERT(m_applied && "Aura effects have not yet been applied");
		ASSERT(m_owner && "Valid owner required");

		m_applied = false;
		m_expireCountdown.cancel();

		for (auto effect : m_effects)
		{
			if (effect)
				effect->misapplyAura();
			else
				break;
		}

		if (hasValidSlot())
		{
			// Update owner fields
			m_owner->setUInt32Value(unit_fields::AuraEffect + m_slot, 0);
		}
	}
	
	UInt16 AuraSpellSlot::addAuraEffect(AuraEffectPtr effect)
	{
		ASSERT(effect && "nullptr not valid for parameter effect");
		ASSERT(!m_applied && "Spell effects may not be added after an aura has been applied");

		for (UInt16 i = 0; i < m_effects.size(); ++i)
		{
			if (!m_effects[i])
			{
				if (effect->getTotalDuration() > m_totalDuration)
					m_totalDuration = effect->getTotalDuration();

				m_effects[i] = effect;
				return i;
			}
		}

		ASSERT(false && "AuraSpellSlot has already 3 aura effects in use, which is max");
		return 0;
	}
	
	void AuraSpellSlot::setOwner(std::shared_ptr<GameUnit> owner)
	{
		// Remember if effects have been applied
		const bool wasApplied = m_applied;

		// Misapply effects from previous owner
		if (m_applied && m_owner)
		{
			misapplyEffects();
			m_owner = nullptr;
		}

		// Setup new owner
		m_owner = owner;

		// Eventually re-apply effects on new owner (if there is any)
		if (wasApplied && m_owner)
		{
			applyEffects();
		}
	}

	void AuraSpellSlot::setCaster(std::shared_ptr<GameUnit> caster)
	{
		m_caster = caster;
	}

	GameUnit * AuraSpellSlot::getCaster() const
	{
		return m_caster.lock().get();
	}

	bool AuraSpellSlot::isPassive() const
	{
		return (m_spell.attributes(0) & game::spell_attributes::Passive);
	}
	bool AuraSpellSlot::isPositive() const
	{
		return !isNegative();
	}
	bool AuraSpellSlot::isNegative() const
	{
		return m_spell.positive() == 0;
	}
	bool AuraSpellSlot::isDeathPersistent() const
	{
		return isPassive() || (m_spell.attributes(3) & game::spell_attributes_ex_c::DeathPersistent) != 0;
	}
	bool AuraSpellSlot::hasMechanics(UInt32 mechanicMask) const
	{
		if (mechanicMask & (1 << m_spell.mechanic()))
			return true;

		for (auto &effect : m_effects)
		{
			if (effect)
			{
				if (effect->getEffect().mechanic() & (1 << m_spell.mechanic()))
				{
					return true;
				}
			}
			else
			{
				return false;
			}
		}

		return false;
	}
	bool AuraSpellSlot::hasEffect(UInt32 auraType) const
	{
		for (auto effect : m_effects)
		{
			if (!effect)
				return false;

			if (effect->getEffect().aura() == auraType)
				return true;
		}

		return false;
	}
	void AuraSpellSlot::setSlot(UInt8 slot)
	{
		ASSERT(!m_applied && "Slot may not be changed while the aura is applied");

		m_slot = slot;
	}
	bool AuraSpellSlot::shouldOverwriteAura(AuraSpellSlot & other)
	{
		if (&other == this)
			return true;

		// Check if both spells share the same base id
		const bool sameSpellId = other.getSpell().baseid() == m_spell.baseid();
		// Check if this spell may be applied by multiple casters
		const bool stackForDiffCasters = (m_spell.attributes(3) & game::spell_attributes_ex_c::StackForDiffCasters) != 0;
		// Check if casters match
		const bool sameCaster = (other.getCaster() == getCaster());
		// Check if same item guid
		const bool sameItem = (other.getItemGuid() == getItemGuid());
		// Check if same family
		const bool sameFamily = (other.getSpell().family() == m_spell.family());

		// Same spell by same item?
		if (getItemGuid() != 0 && sameItem && sameSpellId)
			return true;

		// Same spell by same caster?
		if (getCaster() != nullptr && sameCaster && sameSpellId)
			return true;

		// Stackable by different casters never overwrites even if same spell id
		if (sameSpellId && getCaster() && !sameCaster && stackForDiffCasters)
			return false;

		// Now check spell family
		if (sameFamily)
		{
			// Warlock curses should overwrite each other if from same caster
			if (m_spell.family() == game::spell_family::Warlock && sameFamily &&										// Warlock spell family?
				sameCaster && getCaster() &&																			// Same known caster?
				m_spell.dispel() == game::aura_dispel_type::Curse && other.getSpell().dispel() == m_spell.dispel())		// Curse dispel type?
			{
				// Curses do overwrite
				return true;
			}
		}

		return false;
	}
	bool AuraSpellSlot::removeProcCharges(UInt8 count/* = 1*/)
	{
		ASSERT(m_owner && "Valid owner has to exist");
		ASSERT(m_applied && "Aura has to be applied");
		ASSERT(count >= 1 && "At least one charge should be given");

		if (m_procCharges >= count)
		{
			m_procCharges -= count;
			if (m_procCharges == 0)
				m_owner->getAuras().removeAura(*this);
			else
				updateAuraApplication();
		}
		else
		{
			return false;
		}

		return true;
	}
	void AuraSpellSlot::addStack(AuraSpellSlot & aura)
	{
		ASSERT(m_owner && "Valid owner requird");
		ASSERT(m_applied && "Aura has to be applied");
		ASSERT(aura.getSpell().id() == m_spell.id() && "Aura spells have to match");

		if (m_spell.stackamount() != m_stackCount)
		{
			m_stackCount++;
			updateAuraApplication();

			// Now for every effect, update their base points and reapply them
			for (Int32 i = 0; i < m_effects.size(); ++i)
			{
				if (!m_effects[i])
					break;

				ASSERT((m_effects[i] && aura.m_effects[i]) && "Both auras have to have the same amount of effects");
				ASSERT((m_effects[i]->getEffect().aura() && aura.m_effects[i]->getEffect().aura()) && "Both effects have to match");

				Int32 basePoints = m_stackCount * aura.m_effects[i]->getBasePoints();
				m_effects[i]->setBasePoints(basePoints);
			}
		}
		else
		{
			for (Int32 i = 0; i < m_effects.size(); ++i)
			{
				if (!m_effects[i])
					break;

				// Cause the effect to reset tick count etc.
				m_effects[i]->setBasePoints(m_effects[i]->getBasePoints());
			}
		}

		// Reset aura expiration (if any)
		if (m_totalDuration > 0)
			m_expireCountdown.setEnd(getCurrentTime() + m_totalDuration);

		if (hasValidSlot())
		{
			m_owner->auraUpdated(m_slot, m_spell.id(), getTotalDuration(), getTotalDuration());

			auto *caster = getCaster();
			if (caster)
			{
				caster->targetAuraUpdated(m_owner->getGuid(), m_slot,
					m_spell.id(), getTotalDuration(), getTotalDuration());
			}
		}
	}
	void AuraSpellSlot::updateAuraApplication()
	{
		ASSERT(hasValidSlot() && "Valid slot required");
		ASSERT(m_owner && "Valid owner required");
		ASSERT(m_applied && "Aura needs to be applied first");

		const UInt32 stackCount = m_procCharges > 0 ? m_procCharges * m_stackCount : m_stackCount;
		const UInt32 index = m_slot / 4;
		const UInt32 byte = (m_slot % 4) * 8;

		UInt32 val = m_owner->getUInt32Value(unit_fields::AuraApplications + index);
		val &= ~(0xFF << byte);
		val |= ((UInt8(stackCount <= 255 ? stackCount - 1 : 255 - 1)) << byte);
		m_owner->setUInt32Value(unit_fields::AuraApplications + index, val);
	}
	void AuraSpellSlot::forEachEffect(std::function<bool(AuraEffectPtr)> functor)
	{
		for (auto effect : m_effects)
		{
			if (!effect)
				return;

			if (!functor(effect))
				return;
		}
	}
	void AuraSpellSlot::forEachEffectOfType(UInt32 type, std::function<bool(AuraEffectPtr)> functor)
	{
		for (auto effect : m_effects)
		{
			if (!effect)
				return;

			if (effect->getEffect().aura() == type)
			{
				if (!functor(effect))
					return;
			}
		}
	}

	void AuraSpellSlot::onExpiration()
	{
		if (!m_owner) {
			return;
		}

		// Notify all effects about the expiration
		for (auto effect : m_effects)
		{
			if (!effect)
				break;

			effect->onExpired();
		}

		// Remove this aura from the owner's aura list
		m_owner->getAuras().removeAura(*this);
	}
}
