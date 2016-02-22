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
			if (a->getCaster() == aura->getCaster() &&
			        a->getSpell().id() == aura->getSpell().id())
			{
				newSlot = a->getSlot();
				isReplacement = true;

				// Replace old aura instance
				if (a->getEffect().aura() == aura->getEffect().aura() &&
				        a->getEffect().index() == aura->getEffect().index())
				{
					// Remove aura - new aura will be added
					it = m_auras.erase(it);
					a->misapplyAura();
				}
				break;
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
		assert(it != m_auras.end());
		auto strong = *it;

		// Remove the aura from the list of auras
		it = m_auras.erase(it);

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
		const auto newEnd =
		    std::partition(std::begin(m_auras),
		                   std::end(m_auras),
		                   [](const std::shared_ptr<Aura> &instance)
		{
			return (instance->getSpell().attributes(3) & 0x00100000) != 0;
		});

		for (auto i = newEnd; i != std::end(m_auras); ++i)
		{
			//TODO handle exceptions somehow?
			(*i)->onForceRemoval();
		}

		//m_auras.erase(newEnd, std::end(m_auras));
	}

	bool AuraContainer::hasAura(game::AuraType type) const
	{
		for (auto &it : m_auras)
		{
			if (it->getEffect().aura() == type)
			{
				return true;
			}
		}

		return false;
	}

	UInt32 AuraContainer::consumeAbsorb(UInt32 damage, UInt8 school)
	{
		UInt32 absorbed = 0;
		UInt32 ownerMana = m_owner.getUInt32Value(unit_fields::Power1);
		UInt32 manaShielded = 0;
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
					it->setBasePoints(0);
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
						it->setBasePoints(0);
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

}
