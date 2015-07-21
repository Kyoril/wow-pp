//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "spell_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"
#include "game/defines.h"
#include "skill_entry.h"
#include "data_load_context.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	SpellEntry::SpellEntry()
		: attributes(0)
		, cooldown(0)
		, castTimeIndex(1)
		, powerType(power_type::Mana)
		, cost(0)
		, maxLevel(0)
		, baseLevel(0)
		, spellLevel(0)
		, speed(0.0f)
		, schoolMask(0.0f)
		, dmgClass(0.0f)
		, itemClass(-1)
		, itemSubClassMask(0)
		, facing(0)
	{
		attributesEx.fill(0);
	}

	bool SpellEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetInteger("attributes", attributes);
		for (size_t i = 0; i < attributesEx.size(); ++i)
		{
			std::stringstream strm;
			strm << "attributes_ex_" << (i + 1);
			wrapper.table.tryGetInteger(strm.str(), attributesEx[i]);
		}
		wrapper.table.tryGetInteger("name", name);
		wrapper.table.tryGetInteger("cast_time", castTimeIndex);
		wrapper.table.tryGetInteger("cooldown", cooldown);
		Int32 powerTypeValue = 0;
		wrapper.table.tryGetInteger("power", powerTypeValue);
		powerType = static_cast<PowerType>(powerTypeValue);
		wrapper.table.tryGetInteger("cost", cost);
		wrapper.table.tryGetInteger("facing", facing);
		wrapper.table.tryGetInteger("max_level", maxLevel);
		wrapper.table.tryGetInteger("base_level", baseLevel);
		wrapper.table.tryGetInteger("spell_level", spellLevel);
		wrapper.table.tryGetInteger("speed", speed);
		wrapper.table.tryGetInteger("school_mask", schoolMask);
		wrapper.table.tryGetInteger("dmg_class", dmgClass);
		wrapper.table.tryGetInteger("item_class", itemClass);
		wrapper.table.tryGetInteger("item_subclass_mask", itemSubClassMask);

		const sff::read::tree::Array<DataFileIterator> *skillsArray = wrapper.table.getArray("skills");
		if (skillsArray)
		{
			for (size_t j = 0, d = skillsArray->getSize(); j < d; ++j)
			{
				UInt32 skillId = skillsArray->getInteger(j, 0);
				if (skillId == 0)
				{
					context.onWarning("Invalid skill id in spell entry - skill will be ignored");
					continue;
				}

				context.loadLater.push_back([skillId, this, &context]() -> bool
				{
					const SkillEntry * skill = context.getSkill(skillId);
					if (skill)
					{
						this->skillsOnLearnSpell.push_back(skill);
						return true;
					}
					else
					{
						WLOG("Unknown skill " << skillId << "  for spell entry " << this->id << " - skill will be ignored!");
						return true;
					}
				});
			}
		}

		const sff::read::tree::Array<DataFileIterator> *effectsArray = wrapper.table.getArray("effects");
		if (effectsArray)
		{
			effects.resize(effectsArray->getSize());
			for (size_t j = 0, d = effectsArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const effectTable = effectsArray->getTable(j);
				if (!effectTable)
				{
					context.onError("Invalid spell effect table");
					return false;
				}

				// Get effect type
				UInt32 effectIndex = 0;
				if (!effectTable->tryGetInteger("type", effectIndex))
				{
					context.onError("Invalid spell effect type");
					return false;
				}

				// Validate effect
				if (effectIndex == game::spell_effects::Invalid_ ||
					effectIndex > game::spell_effects::Count_)
				{
					context.onError("Invalid spell effect type");
					return false;
				}

				Effect &effect = effects[j];
				effect.type = static_cast<game::SpellEffect>(effectIndex);
				effectTable->tryGetInteger("base_points", effect.basePoints);
				effectTable->tryGetInteger("base_dice", effect.baseDice);
				effectTable->tryGetInteger("die_sides", effect.dieSides);
				effectTable->tryGetInteger("mechanic", effect.mechanic);
				effectTable->tryGetInteger("target_a", effect.targetA);
				effectTable->tryGetInteger("target_b", effect.targetB);
				effectTable->tryGetInteger("per_combo_point", effect.pointsPerComboPoint);
			}
		}

		return true;
	}

	void SpellEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		if (attributes) context.table.addKey("attributes", attributes);
		for (size_t i = 0; i < attributesEx.size(); ++i)
		{
			if (attributesEx[i])
			{
				std::stringstream strm;
				strm << "attributes_ex_" << (i + 1);
				context.table.addKey(strm.str(), attributesEx[i]);
			}
		}
		if (castTimeIndex != 1) context.table.addKey("cast_time", castTimeIndex);
		if (cooldown != 0) context.table.addKey("cooldown", cooldown);
		if (powerType != power_type::Mana) context.table.addKey("power", static_cast<Int32>(powerType));
		if (cost != 0) context.table.addKey("cost", cost);
		if (facing != 0) context.table.addKey("facing", facing);
		if (maxLevel != 0) context.table.addKey("max_level", maxLevel);
		if (baseLevel != 0) context.table.addKey("base_level", baseLevel);
		if (spellLevel != 0) context.table.addKey("spell_level", spellLevel);
		if (speed != 0.0f) context.table.addKey("speed", speed);
		if (schoolMask != 0) context.table.addKey("school_mask", schoolMask);
		if (dmgClass != 0) context.table.addKey("dmg_class", dmgClass);
		if (itemClass != -1) context.table.addKey("item_class", itemClass);
		if (itemSubClassMask != 0) context.table.addKey("item_subclass_mask", itemSubClassMask);

		// Write skills
		if (!skillsOnLearnSpell.empty())
		{
			sff::write::Array<char> skillsArray(context.table, "skills", sff::write::Comma);
			{
				for (auto &skill : skillsOnLearnSpell)
				{
					skillsArray.addElement(skill->id);
				}
			}
			skillsArray.finish();
		}

		// Write spell effects
		if (!effects.empty())
		{
			sff::write::Array<char> effectsArray(context.table, "effects", sff::write::MultiLine);
			{
				for (auto &effect : effects)
				{
					sff::write::Table<char> effectTable(effectsArray, sff::write::Comma);
					{
						effectTable.addKey("type", static_cast<UInt32>(effect.type));
						if (effect.basePoints != 0) effectTable.addKey("base_points", effect.basePoints);
						if (effect.baseDice != 0) effectTable.addKey("base_dice", effect.baseDice);
						if (effect.dieSides != 0) effectTable.addKey("die_sides", effect.dieSides);
						if (effect.mechanic != 0) effectTable.addKey("mechanic", effect.mechanic);
						if (effect.targetA != 0) effectTable.addKey("target_a", effect.targetA);
						if (effect.targetB != 0) effectTable.addKey("target_b", effect.targetB);
						if (effect.pointsPerComboPoint != 0) effectTable.addKey("per_combo_point", effect.pointsPerComboPoint);
					}
					effectTable.finish();
				}
			}
			effectsArray.finish();
		}
	}
}
