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

#include "common/typedefs.h"
#include "templates/template_manager.h"
#include "map_entry.h"
#include "class_entry.h"
#include "race_entry.h"
#include "level_entry.h"
#include "creature_type_entry.h"
#include "unit_entry.h"
#include "cast_time_entry.h"
#include "spell_entry.h"
#include <boost/noncopyable.hpp>
#include <memory>

namespace wowpp
{
	template <class T>
	struct MakeTemplateManager
	{
		typedef TemplateManager<T, std::unique_ptr<T>> type;
	};


	typedef MakeTemplateManager<CastTimeEntry>::type CastTimeEntryManager;
	typedef MakeTemplateManager<SpellEntry>::type SpellEntryManager;
	typedef MakeTemplateManager<MapEntry>::type MapEntryManager;
	typedef MakeTemplateManager<LevelEntry>::type LevelEntryManager;
	typedef MakeTemplateManager<ClassEntry>::type ClassEntryManager;
	typedef MakeTemplateManager<RaceEntry>::type RaceEntryManager;
	typedef MakeTemplateManager<CreatureTypeEntry>::type CreatureTypeEntryManager;
	typedef MakeTemplateManager<UnitEntry>::type UnitEntryManager;


	class Project : private boost::noncopyable
	{
	public:

		CastTimeEntryManager castTimes;
		SpellEntryManager spells;
		MapEntryManager maps;
		LevelEntryManager levels;
		ClassEntryManager classes;
		RaceEntryManager races;
		CreatureTypeEntryManager creaturetypes;
		UnitEntryManager units;

	public:

		Project();

		bool load(
		    const String &directory);
		bool save(
			const String &directory);
	};
}
