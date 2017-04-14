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
#include "aura_container.h"
#include "aura_spell_slot.h"
#include "aura_effect.h"
#include "game_unit.h"
#include "log/default_log_levels.h"
#include "common/linear_set.h"
#include "common/macros.h"

namespace wowpp
{
	AuraContainer::AuraContainer(GameUnit &owner)
		: m_owner(owner)
	{
	}

	bool AuraContainer::addAura(AuraPtr aura)
	{
		// If aura shouldn't be hidden on the client side...
		if (!aura->isPassive() && (aura->getSpell().attributes(0) & game::spell_attributes::HiddenClientSide) == 0)
		{
			UInt8 newSlot = 0xFF;

			// Check old auras for new slot
			for (auto it = m_auras.begin(); it != m_auras.end();)
			{
				if ((*it)->hasValidSlot() && aura->shouldOverwriteAura(*(*it)))
				{
					// Only overwrite if higher or equal base rank
					const bool sameSpell = ((*it)->getSpell().baseid() == aura->getSpell().baseid());
					if ((sameSpell && (*it)->getSpell().rank() <= aura->getSpell().rank()) || !sameSpell)
					{
						newSlot = (*it)->getSlot();
						removeAura(it);
						break;
					}
					else if (sameSpell)
					{
						// Aura should have been overwritten, but has a lesser rank
						// So stop checks here and don't apply new aura at all
						// TODO: Send proper error message to the client
						return false;
					}
					else
					{
						++it;
					}
				}
				else
				{
					++it;
				}
			}

			// Aura hasn't been overwritten yet, determine new slot
			if (newSlot == 0xFF)
			{
				if (aura->isPositive())
				{
					for (UInt8 i = 0; i < 40; ++i)
					{
						if (m_owner.getUInt32Value(unit_fields::AuraEffect + i) == 0)
						{
							newSlot = i;
							break;
						}
					}
				}
				else
				{
					for (UInt8 i = 40; i < 56; ++i)
					{
						if (m_owner.getUInt32Value(unit_fields::AuraEffect + i) == 0)
						{
							newSlot = i;
							break;
						}
					}
				}
			}

			// No more free slots
			if (newSlot == 0xFF)
				return false;

			// Apply slot
			aura->setSlot(newSlot);
		}

		// Remove other shapeshifting auras in case this is a shapeshifting aura
		if (aura->hasEffect(game::aura_type::ModShapeShift))
			removeAurasByType(game::aura_type::ModShapeShift);

		// Add aura
		m_auras.push_back(aura);

		// Increase aura counters
		aura->forEachEffect([&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
			m_auraTypeCount[effect->getEffect().aura()]++;
			return true;
		});

		// Apply aura effects
		aura->applyEffects();
		return true;
	}

	AuraContainer::AuraList::iterator AuraContainer::findAura(AuraSpellSlot &aura)
	{
		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			if (it->get() == &aura)
			{
				return it;
			}
		}

		return m_auras.end();
	}

	void AuraContainer::removeAura(AuraList::iterator &it)
	{
		// Make sure that the aura is not destroy when releasing
		ASSERT(it != m_auras.end());
		auto strong = (*it);
		
		// Remove the aura from the list of auras
		it = m_auras.erase(it);

		// Reduce counter
		strong->forEachEffect([&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
			ASSERT(m_auraTypeCount[effect->getEffect().aura()] > 0 && "At least one aura of this type should still exist");
			m_auraTypeCount[effect->getEffect().aura()]--;
			return true;
		});
		
		// NOW misapply the aura. It is important to call this method AFTER the aura has been
		// removed from the list of auras. First: To prevent a stack overflow when removing
		// an aura causes the remove of the same aura types like in ModShapeShift. Second:
		// Stun effects need to check whether there are still ModStun auras on the target, AFTER
		// the aura has been removed (or else it would count itself)!!
		strong->misapplyEffects();
	}

	void AuraContainer::removeAura(AuraSpellSlot &aura)
	{
		auto it = findAura(aura);
		if (it != m_auras.end())
		{
			removeAura(it);
		}
		else {
			WLOG("Could not find aura to remove!");
		}
	}

	void AuraContainer::handleTargetDeath()
	{
		AuraList::iterator it = m_auras.begin();
		while (it != m_auras.end())
		{
			// Keep passive and death persistent auras
			if ((*it)->isDeathPersistent() || (*it)->isPassive())
			{
				++it;
			}
			else
			{
				// Remove aura
				removeAura(it);
			}
		}
	}

	bool AuraContainer::hasAura(game::AuraType type) const
	{
		auto it = m_auraTypeCount.find(type);
		if (it == m_auraTypeCount.end())
			return false;

		return it->second > 0;
	}

	UInt32 AuraContainer::consumeAbsorb(UInt32 damage, UInt8 school)
	{
		// TODO!
#if 0
		UInt32 absorbed = 0;
		UInt32 ownerMana = m_owner.getUInt32Value(unit_fields::Power1);
		UInt32 manaShielded = 0;

		std::list<std::shared_ptr<AuraEffect>> toRemove;
		for (auto &it : m_auras)
		{
			if (it->getEffect().aura() == game::aura_type::SchoolAbsorb
			        && ((it->getEffect().miscvaluea() & school) != 0))
			{
				UInt32 toConsume = static_cast<UInt32>(it->getBasePoints());
				if (toConsume >= damage)
				{
					absorbed += damage;
					it->setBasePoints(toConsume - damage);
					break;
				}
				else
				{
					absorbed += toConsume;
					toRemove.push_back(it);
				}
			}
			else if (it->getEffect().aura() == game::aura_type::ManaShield
			         && ((it->getEffect().miscvaluea() & school) != 0))
			{
				UInt32 toConsume = static_cast<UInt32>(it->getBasePoints());
				UInt32 toConsumeByMana = (float) ownerMana / it->getEffect().multiplevalue();
				if (toConsume >= damage && toConsumeByMana >= damage)
				{
					absorbed += damage;
					manaShielded += damage;
					const Int32 manaToConsume = (damage * it->getEffect().multiplevalue());
					ownerMana -= manaToConsume;
					it->setBasePoints(toConsume - damage);
					break;
				}
				else
				{
					if (toConsume < toConsumeByMana)
					{
						absorbed += toConsume;
						manaShielded += toConsume;
						const Int32 manaToConsume = (toConsume * it->getEffect().multiplevalue());
						ownerMana -= manaToConsume;
						toRemove.push_back(it);
					}
					else
					{
						absorbed += toConsumeByMana;
						manaShielded += toConsumeByMana;
						ownerMana -= toConsumeByMana * 2;
						it->setBasePoints(toConsume - toConsumeByMana);
					}
				}
			}
		}

		if (manaShielded > 0)
		{
			m_owner.setUInt32Value(unit_fields::Power1, ownerMana);
		}

		for (auto &aura : toRemove)
		{
			removeAura(*aura);
		}
#endif

		return 0;
		//return absorbed;
	}

	Int32 AuraContainer::getMaximumBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			(*it)->forEachEffectOfType(type, [&treshold](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				if (effect->getBasePoints() > treshold)
					treshold = effect->getBasePoints();
				return true;
			});
		}

		return treshold;
	}

	Int32 AuraContainer::getMinimumBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			(*it)->forEachEffectOfType(type, [&treshold](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				if (effect->getBasePoints() < treshold)
					treshold = effect->getBasePoints();
				return true;
			});
		}

		return treshold;
	}

	Int32 AuraContainer::getTotalBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			(*it)->forEachEffectOfType(type, [&treshold](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				treshold += effect->getBasePoints();
				return true;
			});
		}

		return treshold;
	}

	float AuraContainer::getTotalMultiplier(game::AuraType type) const
	{
		float multiplier = 1.0f;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			(*it)->forEachEffectOfType(type, [&multiplier](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				multiplier *= (100.0f + static_cast<float>(effect->getBasePoints())) / 100.0f;
				return true;
			});
		}

		return multiplier;
	}

	void AuraContainer::forEachAura(std::function<bool(AuraEffect&)> functor)
	{
		for (auto aura : m_auras)
		{
			aura->forEachEffect([&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
				return functor(*effect);
			});
		}
	}

	void AuraContainer::forEachAuraOfType(game::AuraType type, std::function<bool(AuraEffect&)> functor)
	{
		// Performance check before iteration
		if (!hasAura(type))
			return;

		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			if ((*it)->hasEffect(type))
			{
				(*it)->forEachEffectOfType(type, [&](AuraSpellSlot::AuraEffectPtr effect) -> bool {
					return functor(*effect);
				});
			}
		}
	}

	void AuraContainer::logAuraInfos()
	{
		// TODO
	}

	void AuraContainer::removeAllAurasDueToSpell(UInt32 spellId)
	{
		ASSERT(spellId && "Valid spell id should be specified");

		AuraList::iterator it = m_auras.begin();
		while (it != m_auras.end())
		{
			if ((*it)->getSpell().id() == spellId)
			{
				removeAura(it);
			}
			else
			{
				it++;
			}
		}
	}

	void AuraContainer::removeAllAurasDueToItem(UInt64 itemGuid)
	{
		ASSERT(itemGuid && "Valid item guid should be specified");

		for (auto it = m_auras.begin(); it != m_auras.end(); )
		{
			if ((*it)->getItemGuid() == itemGuid)
				removeAura(it);
			else
				++it;
		}
	}

	void AuraContainer::removeAllAurasDueToMechanic(UInt32 immunityMask)
	{
		ASSERT(immunityMask && "At least one mechanic should be provided");

		// We need to remove all auras by their spell
		LinearSet<UInt32> spells;
		for (auto it = m_auras.begin(); it != m_auras.end();)
		{
			if ((*it)->hasMechanics(immunityMask))
				removeAura(it);
			else
				++it;
		}
	}

	UInt32 AuraContainer::removeAurasDueToDispel(UInt32 dispelType, bool dispelPositive, UInt32 count/* = 1*/)
	{
		ASSERT(dispelType && "A valid dispel type should be provided");
		ASSERT(count && "At least a number of 1 should be provided");

		UInt32 successCount = 0;

		for (auto it = m_auras.begin(); it != m_auras.end();)
		{
			if ((*it)->getSpell().dispel() == dispelType &&
				(*it)->isPositive() == dispelPositive)
			{
				removeAura(it);
				if (++successCount >= count)
					return successCount;
			}
			else
			{
				++it;
			}
		}

		return successCount;
	}

	void AuraContainer::removeAurasByType(UInt32 auraType)
	{
		ASSERT(auraType && "A valid aura effect type should be specified");

		// We need to remove all auras by their spell
		for (auto it = m_auras.begin(); it != m_auras.end(); )
		{
			if ((*it)->hasEffect(auraType))
				removeAura(it);
			else
				++it;
		}
	}

	void AuraContainer::removeAllAurasDueToInterrupt(game::SpellAuraInterruptFlags flags)
	{
		// We need to remove all auras by their spell
		for (auto it = m_auras.begin(); it != m_auras.end(); )
		{
			if ((*it)->getSpell().aurainterruptflags() & flags)
				removeAura(it);
			else
				++it;
		}
	}

	void AuraContainer::removeAllAuras()
	{
		for (auto it = m_auras.begin(); it != m_auras.end();)
		{
			removeAura(it);
		}
	}
}
