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
#include "data_load_context.h"
#include "common/enum_strings.h"

namespace wowpp
{
	namespace map_instance_type
	{
		enum Type
		{
			/// There is only one global instance of this map overall.
			Global			= 0x00,
			/// There can be many instances, since it's a dungeon (instance can be reset).
			Dungeon			= 0x01,
			/// There can be many instances, since it's a raid (instance resets at specific timestamp).
			Raid			= 0x02,
			/// There can be many cross-realm instances with some logic behind (win/loose)
			Battleground	= 0x03,

			Count_,
			Invalid_ = Count_
		};
	}

	typedef map_instance_type::Type MapInstanceType;

	namespace constant_literal
	{
		typedef EnumStrings<MapInstanceType, map_instance_type::Count_,
			map_instance_type::Invalid_> MapInstanceTypeStrings;
		extern const MapInstanceTypeStrings mapInstanceType;
	}

	struct UnitEntry;


	struct SpawnPlacement
	{
		bool respawn;
		GameTime respawnDelay;
		std::array<float, 3> position;
		float rotation;
		float radius;
		size_t maxCount;
		const UnitEntry *unit;

		SpawnPlacement()
			: respawn(true)
			, respawnDelay(0.0f)
			, rotation(0.0f)
			, radius(0.0f)
			, maxCount(1)
			, unit(nullptr)
		{
		}
	};

	struct ObjectEntry;

	struct ObjectSpawnPlacement
	{
		String name;
		bool respawn;
		GameTime respawnDelay;
		std::array<float, 3> position;
		float orientation;
		float radius;
		std::array<float, 4> rotation;
		UInt32 animProgress;
		UInt32 state;
		size_t maxCount;
		const ObjectEntry *object;

		ObjectSpawnPlacement()
			: respawn(true)
			, respawnDelay(0.0f)
			, orientation(0.0f)
			, radius(0.0f)
			, maxCount(1)
			, object(nullptr)
		{
		}
	};


	struct MapEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;
		typedef std::vector<SpawnPlacement> Spawns;
		typedef std::vector<ObjectSpawnPlacement> ObjectSpawns;

		Spawns spawns;
		ObjectSpawns objectSpawns;
		MapInstanceType instanceType;
		String directory;
		String name;

		MapEntry();
		bool load(DataLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
