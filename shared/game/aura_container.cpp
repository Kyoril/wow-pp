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

#include "aura_container.h"
#include "game_unit.h"
#include "log/default_log_levels.h"
#include "data/spell_entry.h"
#include <algorithm>
#include <cassert>

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
			auto &a = *it;
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
				if (aura->isPositive())
					val |= (UInt32(31) << byte);
				else
					val |= (UInt32(9) << byte);
				m_owner.setUInt32Value(unit_fields::AuraFlags + index, val);

				// Notify caster
				m_owner.auraUpdated(newSlot, aura->getSpell().id(), aura->getSpell().duration(), aura->getSpell().maxduration());

				if (aura->getCaster())
				{
					aura->getCaster()->targetAuraUpdated(m_owner.getGuid(), newSlot,
						aura->getSpell().id(), aura->getSpell().duration(), aura->getSpell().maxduration());
				}
			}
		}

		// Add aura to the list of auras of this unit and apply it's effects
		aura->applyAura();

		// Store aura instance
		m_auras.push_back(std::move(aura));

		return true;
	}

	void AuraContainer::removeAura(size_t index)
	{
		assert(index < getSize());

		using std::swap;

		swap(m_auras.back(), m_auras[index]);
		m_auras.pop_back();
	}

	size_t AuraContainer::findAura(Aura &aura, size_t begin)
	{
		assert(begin <= getSize());

		const auto i = std::find_if(std::begin(m_auras) + begin,
			std::end(m_auras),
			[&aura](const std::shared_ptr<Aura> &instance) -> bool
		{
			assert(instance);
			return instance.get() == &aura;
		});

		return static_cast<size_t>(std::distance(std::begin(m_auras), i));
	}

	Aura & AuraContainer::get(size_t index)
	{
		assert(index < getSize());
		return *m_auras[index];
	}

	const Aura & AuraContainer::get(size_t index) const
	{
		assert(index < getSize());
		return *m_auras[index];
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

		m_auras.erase(newEnd, std::end(m_auras));
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
			DLOG("Mana shielded: " << manaShielded);
			m_owner.setUInt32Value(unit_fields::Power1, ownerMana);
		}
		return absorbed;
	}

	void AuraContainer::removeAllAurasDueToSpell(UInt32 spellId)
	{
		size_t index = 0;
		auto it = m_auras.begin();
		while (it != m_auras.end())
		{
			if ((*it)->getSpell().id() == spellId)
			{
				removeAura(index);
			}
			else
			{
				++index; ++it;
			}
		}
	}

	boost::optional<std::size_t> findAuraInstanceIndex(AuraContainer &instances, Aura &instance)
	{
		for (size_t i = 0, c = instances.getSize(); i < c; ++i)
		{
			if (&instance == &instances.get(i))
			{
				return i;
			}
		}
		return boost::optional<std::size_t>();
	}

}
