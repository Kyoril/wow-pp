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
#include "single_cast_state.h"
#include "game_character.h"

namespace wowpp
{
	void SingleCastState::spellScriptEffectEnrage(const proto::SpellEffect &effect)
	{
		auto &caster = m_cast.getExecuter();
		if (caster.getByteValue(unit_fields::Bytes2, 3) == game::shapeshift_form::Bear)
		{
			m_basePoints[1] = -27;
		}
		else if (caster.getByteValue(unit_fields::Bytes2, 3) == game::shapeshift_form::DireBear)
		{
			m_basePoints[1] = -16;
		}

		spellEffectApplyAura(effect);
	}

	void SingleCastState::spellScriptEffectExecute(const proto::SpellEffect &effect)
	{
		// Rage has already been reduced by executing this spell, though the remaining value is the rest
		m_cast.getExecuter().castSpell(
			m_target, 20647, { static_cast<Int32>(calculateEffectBasePoints(effect) + m_cast.getExecuter().getUInt32Value(unit_fields::Power2) * effect.dmgmultiplier()), 0, 0 });
		m_cast.getExecuter().setUInt32Value(unit_fields::Power2, 0);
	}

	void SingleCastState::spellScriptEffectLifeTap(const proto::SpellEffect &effect)
	{
		// Determine current amount of cost
		float cost = calculateEffectBasePoints(effect);

		// Used frequently
		auto &executer = m_cast.getExecuter();

		// Apply spell cost modifications
		if (executer.isGameCharacter())
		{
			reinterpret_cast<GameCharacter&>(executer).applySpellMod(spell_mod_op::Cost, m_spell.id(), cost);
		}

		UInt32 dmg = cost;
		executer.applyDamageDoneBonus(m_spell.schoolmask(), 0, dmg);
		executer.applyDamageTakenBonus(m_spell.schoolmask(), 0, dmg);

		// Check casters health amount to prevent suicide
		UInt32 health = executer.getUInt32Value(unit_fields::Health);
		if (health > dmg)
		{
			// Deal damage
			executer.setUInt32Value(unit_fields::Health, health - dmg);

			// Add mana
			Int32 mana = static_cast<Int32>(dmg);

			// TODO: Improved life tap mod

			// Regenerate mana
			executer.castSpell(m_target, 31818, { mana, 0, 0 }, 0, true);
		}
		else
		{
			executer.spellCastError(m_spell, game::spell_cast_result::FailedFizzle);
		}
	}

	void SingleCastState::spellScriptEffectCreateHealthstone(const proto::SpellEffect &effect)
	{
		// Used frequently
		auto &executer = m_cast.getExecuter();

		// Only usable as character
		if (!executer.isGameCharacter())
			return;

		auto &character = reinterpret_cast<GameCharacter&>(executer);

		// Add support for improved healthstone talent
		UInt32 itemtype = 0, rank = 0;
		if (character.hasSpell(18693))
			rank = 2;
		else if (character.hasSpell(18692))
			rank = 1;

		// Enumerate item ids
		static UInt32 const itypes[5][3] =
		{
			{ 5512, 19004, 19005 },              // Minor Healthstone
			{ 5511, 19006, 19007 },              // Lesser Healthstone
			{ 5509, 19008, 19009 },              // Healthstone
			{ 5510, 19010, 19011 },              // Greater Healthstone
			{ 9421, 19012, 19013 }               // Major Healthstone
		};

		switch (m_spell.id())
		{
			case  6201:
				itemtype = itypes[0][rank]; break; // Minor Healthstone
			case  6202:
				itemtype = itypes[1][rank]; break; // Lesser Healthstone
			case  5699:
				itemtype = itypes[2][rank]; break; // Healthstone
			case 11729:
				itemtype = itypes[3][rank]; break; // Greater Healthstone
			case 11730:
				itemtype = itypes[4][rank]; break; // Major Healthstone
			default:
				return;
		}

		// Create the item
		createItems(character, itemtype, 1);
	}
}
