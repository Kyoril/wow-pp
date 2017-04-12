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
#include "aura.h"
#include "game_unit.h"
#include "log/default_log_levels.h"
#include "common/linear_set.h"

namespace wowpp
{
	AuraContainer::AuraContainer(GameUnit &owner)
		: m_owner(owner)
	{
	}

	bool AuraContainer::addAura(std::shared_ptr<Aura> aura)
	{
		// Find the new aura slot to be used
		UInt8 newSlot = 0xFF;
		bool isReplacement = false;
		for (auto it = m_auras.begin(); it != m_auras.end(); ++it)
		{
			// Same spell, same caster and same effect index - don't stack!
			auto a = *it;

			// Checks if both auras are from the same caster
			const bool isSameCaster = a->getCaster() == aura->getCaster() && a->getItemGuid() == aura->getItemGuid();

			// Checks if both auras have the same spell family (&flags)
			const bool isSameFamily = 
				(a->getSpell().family() == aura->getSpell().family() && a->getSpell().familyflags() == aura->getSpell().familyflags());

			// Check if this spell has a specific family set
			//const bool hasFamily =
			//	(a->getSpell().family() != 0);

			// Check if this is the same spell (even if they don't have the same spell id)
			const bool isSameSpell = 
				(a->getSpell().baseid() == aura->getSpell().baseid() &&
				(/*hasFamily && */isSameFamily)) &&
				a->isPassive() == aura->isPassive();

			// Checks if the new aura stacks for different casters (can have multiple auras by different casters even though it's the same spell)
			const bool stackForDiffCasters =
				aura->getSpell().attributes(3) & game::spell_attributes_ex_c::StackForDiffCasters ||
				aura->getItemGuid() != 0;

			// Perform checks
			if (isSameSpell &&
				(isSameCaster || !stackForDiffCasters) &&
				!aura->isPassive() && 
				!a->isPassive())
			{
				// Replacement: Use old auras slot for new aura, old aura should be misapplied by this
				newSlot = a->getSlot();
				isReplacement = true;

				// Replace old aura instance if not higher base points
				if (a->getEffect().aura() == aura->getEffect().aura() &&
					a->getEffect().index() == aura->getEffect().index())
				{
					if (a->getSpell().stackamount())
					{
						a->updateStackCount(aura->getBasePoints());

						// Notify caster
						m_owner.auraUpdated(newSlot, a->getSpell().id(), a->getTotalDuration(), a->getTotalDuration());

						if (a->getCaster())
						{
							a->getCaster()->targetAuraUpdated(m_owner.getGuid(), newSlot,
								a->getSpell().id(), a->getTotalDuration(), a->getTotalDuration());
						}

						return false;
					}
					else
					{
						// Remove old aura - new aura will be added
						removeAura(it);
						break;
					}
				}
			}
		}

		if (!aura->isPassive() || (aura->getSpell().attributes(3) & 0x10000000))
		{
			if (!isReplacement)
			{
				if (aura->isPositive())
				{
					for (UInt8 i = 0; i < 40; ++i)
					{
						if (m_owner.getUInt32Value(unit_fields::Aura + i) == 0)
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
						if (m_owner.getUInt32Value(unit_fields::Aura + i) == 0)
						{
							newSlot = i;
							break;
						}
					}
				}
			}

			if (newSlot != 0xFF)
			{
				aura->setSlot(newSlot);
				m_owner.setUInt32Value(unit_fields::Aura + newSlot, aura->getSpell().id());

				UInt32 index = newSlot / 4;
				UInt32 byte = (newSlot % 4) * 8;
				UInt32 val = m_owner.getUInt32Value(unit_fields::AuraLevels + index);
				val &= ~(0xFF << byte);
				val |= ((aura->getCaster() ? aura->getCaster()->getLevel() : 70) << byte);
				m_owner.setUInt32Value(unit_fields::AuraLevels + index, val);

				val = m_owner.getUInt32Value(unit_fields::AuraFlags + index);
				val &= ~(0xFF << byte);
				if (aura->isPositive()) {
					val |= (UInt32(31) << byte);
				}
				else {
					val |= (UInt32(9) << byte);
				}
				m_owner.setUInt32Value(unit_fields::AuraFlags + index, val);

				// Notify caster
				m_owner.auraUpdated(newSlot, aura->getSpell().id(), aura->getTotalDuration(), aura->getTotalDuration());

				if (aura->getCaster())
				{
					aura->getCaster()->targetAuraUpdated(m_owner.getGuid(), newSlot,
					                                     aura->getSpell().id(), aura->getTotalDuration(), aura->getTotalDuration());
				}
			}
		}

		// Remove shapeshifting auras in case of shapeshift aura
		if (aura->getEffect().aura() == game::aura_type::ModShapeShift)
		{
			removeAurasByType(game::aura_type::ModShapeShift);
		}

		// Store aura instance
		auto *auraPtr = aura.get();
		m_auras.push_back(std::move(aura));
		m_auraTypeCount[auraPtr->getEffect().aura()]++;

		// Add aura to the list of auras of this unit and apply it's effects
		// Note: We use auraPtr here, since std::move will make aura invalid
		auraPtr->applyAura();

		return true;
	}

	AuraContainer::AuraList::iterator AuraContainer::findAura(Aura &aura)
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
		auto strong = *it;
		
		// Remove the aura from the list of auras
		it = m_auras.erase(it);

		// Reduce counter
		ASSERT(m_auraTypeCount[strong->getEffect().aura()] > 0);
		m_auraTypeCount[strong->getEffect().aura()]--;

		// NOW misapply the aura. It is important to call this method AFTER the aura has been
		// removed from the list of auras. First: To prevent a stack overflow when removing
		// an aura causes the remove of the same aura types like in ModShapeShift. Second:
		// Stun effects need to check whether there are still ModStun auras on the target, AFTER
		// the aura has been removed (or else it would count itself)!!
		strong->misapplyAura();
	}

	void AuraContainer::removeAura(Aura &aura)
	{
		auto it = findAura(aura);
		if (it != m_auras.end())
		{
			removeAura(it);
		}
	}

	void AuraContainer::handleTargetDeath()
	{
		AuraList::iterator it = m_auras.begin();
		while (it != m_auras.end())
		{
			if (((*it)->getSpell().attributes(3) & game::spell_attributes_ex_c::DeathPersistent) != 0 || 
				(*it)->isPassive())
			{
				++it;
			}
			else
			{
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
		UInt32 absorbed = 0;
		UInt32 ownerMana = m_owner.getUInt32Value(unit_fields::Power1);
		UInt32 manaShielded = 0;

		std::list<std::shared_ptr<Aura>> toRemove;
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

		return absorbed;
	}

	Int32 AuraContainer::getMaximumBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;

		for (auto &it : m_auras)
		{
			if (it->getEffect().aura() == type &&
			        it->getBasePoints() > treshold)
			{
				treshold = it->getBasePoints();
			}
		}

		return treshold;
	}

	Int32 AuraContainer::getMinimumBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;

		for (auto &it : m_auras)
		{
			if (it->getEffect().aura() == type &&
			        it->getBasePoints() < treshold)
			{
				treshold = it->getBasePoints();
			}
		}

		return treshold;
	}

	Int32 AuraContainer::getTotalBasePoints(game::AuraType type) const
	{
		Int32 treshold = 0;
		for (auto &it : m_auras)
		{
			if (it->getEffect().aura() == type)
			{
				treshold += it->getBasePoints();
			}
		}

		return treshold;
	}

	float AuraContainer::getTotalMultiplier(game::AuraType type) const
	{
		float multiplier = 1.0f;
		for (auto &it : m_auras)
		{
			if (it->getEffect().aura() == type)
			{
				multiplier *= (100.0f + static_cast<float>(it->getBasePoints())) / 100.0f;
			}
		}

		return multiplier;
	}

	void AuraContainer::forEachAura(std::function<bool(Aura&)> functor)
	{
		for (auto &aura : m_auras)
		{
			if (!functor(*aura))
			{
				return;
			}
		}
	}

	void AuraContainer::forEachAuraOfType(game::AuraType type, std::function<bool(Aura&)> functor)
	{
		// Performance check before iteration
		if (!hasAura(type))
			return;

		// TODO: Order auras differently so that we iterate as less auras as possible for
		// performance reasons.
		for (auto &aura : m_auras)
		{
			if (aura->getEffect().aura() == type)
			{
				if (!functor(*aura))
				{
					return;
				}
			}
		}
	}

	void AuraContainer::logAuraInfos()
	{
		DLOG("AURA LIST OF TARGET 0x" << std::hex << std::setw(16) << std::uppercase << std::setfill('0') << m_owner.getGuid() << " - " << m_owner.getName())
		for (auto aura : m_auras)
		{
			DLOG("\tAURA " << game::constant_literal::auraTypeNames.getName(static_cast<game::AuraType>(aura->getEffect().aura())) << "\tSPELL " << aura->getSpell().id() << "\tITEM 0x" << std::hex << aura->getItemGuid());
		}
	}

	void AuraContainer::removeAllAurasDueToSpell(UInt32 spellId)
	{
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
		AuraList::iterator it = m_auras.begin();
		while (it != m_auras.end())
		{
			if ((*it)->getItemGuid() == itemGuid)
			{
				removeAura(it);
			}
			else
			{
				it++;
			}
		}
	}

	void AuraContainer::removeAllAurasDueToMechanic(UInt32 immunityMask)
	{
		// We need to remove all auras by their spell
		LinearSet<UInt32> spells;
		for (auto &aura : m_auras)
		{
			if ((immunityMask & (1 << aura->getEffect().mechanic())) != 0 ||
				(immunityMask & (1 << aura->getSpell().mechanic())) != 0)
			{
				spells.optionalAdd(aura->getSpell().id());
			}
		}

		for (auto &spellid : spells)
		{
			removeAllAurasDueToSpell(spellid);
		}
	}

	UInt32 AuraContainer::removeAurasDueToDispel(UInt32 dispelType, UInt32 count)
	{
		UInt32 successCount = 0;

		// Repeat the procedure below N times
		for (UInt32 i = 0; i < count; ++i)
		{
			// We need these variables to remember some aura attributes. One spell can cause multiple
			// auras on a target. So we have to find all auras, whose spell matches the dispel type,
			// but also whose spell ids are the same as the first match and whose caster guid matches
			UInt64 casterGuid = 0;
			UInt32 spell = 0;

			// Aura iteration
			for (auto it = m_auras.begin(); it != m_auras.end(); )
			{
				// If this aura matches the dispel criteria...
				if ((*it)->getSpell().dispel() == dispelType)
				{
					// Did we find a spell?
					if (spell == 0)
					{
						// Remember spell attributes
						spell = (*it)->getSpell().id();
						casterGuid = (*it)->getCasterGuid();
						removeAura(it);

						// We removed at least one spell's auras
						successCount++;
						continue;
					}
					else if (spell == (*it)->getSpell().id())
					{
						// Same spell - validate that it's the same caster
						if (casterGuid == (*it)->getCasterGuid())
						{
							removeAura(it);
							continue;
						}
					}
				}

				// Next aura
				++it;
			}

			return successCount;
		}
	}

	void AuraContainer::removeAurasByType(UInt32 auraType)
	{
		// We need to remove all auras by their spell
		LinearSet<UInt32> spells;
		for (auto &aura : m_auras)
		{
			if (aura->getEffect().aura() == auraType)
			{
				spells.optionalAdd(aura->getSpell().id());
			}
		}

		for (auto &spellid : spells)
		{
			removeAllAurasDueToSpell(spellid);
		}
	}

	Aura *AuraContainer::popBack(UInt8 dispelType, bool positive)
	{
		auto it = m_auras.rbegin();
		while (it != m_auras.rend())
		{
			if ((*it)->isPositive() == positive && (*it)->getSpell().dispel() == dispelType)
			{
				std::shared_ptr<Aura> aura = *it;
				m_auras.erase(std::next(it).base());
				aura->misapplyAura();

				return aura.get();
			}
			else
			{
				it++;
			}
		}

		return nullptr;
	}

	void AuraContainer::removeAllAurasDueToInterrupt(game::SpellAuraInterruptFlags flags)
	{
		// We need to remove all auras by their spell
		LinearSet<UInt32> spells;
		for (auto &aura : m_auras)
		{
			if ((aura->getSpell().aurainterruptflags() & flags) != 0)
			{
				spells.optionalAdd(aura->getSpell().id());
			}
		}

		for (auto &spellid : spells)
		{
			removeAllAurasDueToSpell(spellid);
		}
	}

	void AuraContainer::removeAllAuras()
	{
		auto it = m_auras.begin();
		while(it != m_auras.end())
		{
			std::shared_ptr<Aura> aura = *it;
			it = m_auras.erase(it);

			aura->misapplyAura();
		}
	}
}
