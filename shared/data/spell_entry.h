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

#pragma once

#include "templates/basic_template.h"
#include "game/defines.h"
#include "class_entry.h"

namespace wowpp
{
	struct SpellEntry : BasicTemplate<UInt32>
	{
		/// Represents data needed by a spell effect.
		struct Effect
		{
			game::SpellEffect type;
			Int32 basePoints;
			Int32 dieSides;
			Int32 baseDice;
			float dicePerLevel;
			float pointsPerLevel;
			UInt32 mechanic;
			UInt32 targetA;
			UInt32 targetB;
			UInt32 radiusIndex;
			UInt32 auraName;
			Int32 amplitude;
			float multipleValue;
			UInt32 chainTarget;
			UInt32 itemType;
			Int32 miscValueA;
			Int32 miscValueB;
			UInt32 triggerSpell;
			Int32 pointsPerComboPoint;

			Effect()
				: type(game::spell_effects::Invalid_)
				, basePoints(0)
				, dieSides(0)
				, baseDice(0)
				, dicePerLevel(0.0f)
				, pointsPerLevel(0.0f)
				, mechanic(0)
				, targetA(0)
				, targetB(0)
				, radiusIndex(0)
				, auraName(0)
				, amplitude(0)
				, multipleValue(0.0f)
				, chainTarget(0)
				, itemType(0)
				, miscValueA(0)
				, miscValueB(0)
				, triggerSpell(0)
				, pointsPerComboPoint(0)
			{
			}
		};

		typedef BasicTemplate<UInt32> Super;
		typedef std::vector<Effect> SpellEffects;

		UInt32 attributes;
		UInt32 attributesEx[6];
		String name;
		SpellEffects effects;
		GameTime cooldown;
		UInt32 castTimeIndex;
		PowerType powerType;
		UInt32 cost;

		SpellEntry();
		bool load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
