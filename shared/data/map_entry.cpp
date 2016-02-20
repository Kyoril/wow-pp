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

#include "map_entry.h"
#include "templates/basic_template_save_context.h"
#include "unit_entry.h"
#include "object_entry.h"

namespace wowpp
{
	namespace constant_literal
	{
		const MapInstanceTypeStrings::StringArray strings =
		{{
				"global",
				"dungeon",
				"raid",
				"battleground",
				"arena"
			}
		};


		static_assert(map_instance_type::Global == 0, "");
		static_assert(map_instance_type::Dungeon == 1, "");
		static_assert(map_instance_type::Raid == 2, "");
		static_assert(map_instance_type::Battleground == 3, "");
		static_assert(map_instance_type::Arena == 4, "");
		static_assert(map_instance_type::Count_ == 5, "");

		const MapInstanceTypeStrings mapInstanceType(strings);
	}


	MapEntry::MapEntry()
	{
	}

	bool MapEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		// Load name and directory
		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetString("directory", directory);

		// Load instance type
		instanceType =
		    constant_literal::mapInstanceType.getIdentifier(wrapper.table.getString("instanceType"));
		if (instanceType == map_instance_type::Invalid_)
		{
			context.onError("Invalid map instance type");
			return false;
		}

		if (const sff::read::tree::Array<DataFileIterator> *const spawnArray = wrapper.table.getArray("creature_spawns"))
		{
			for (size_t j = 0, d = spawnArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const spawnTable = spawnArray->getTable(j);
				if (!spawnTable)
				{
					context.onError("Invalid spawn");
					return false;
				}

				SpawnPlacement spawn;

				{
					const sff::read::tree::Array<DataFileIterator> *const positionArray = spawnTable->getArray("position");
					if (!positionArray ||
					        !loadPosition(spawn.position, *positionArray))
					{
						context.onError("Invalid position in a spawn");
						return false;
					}

					spawnTable->tryGetInteger("rotation", spawn.rotation);
				}

				spawn.maxCount = spawnTable->getInteger("count", spawn.maxCount);

				UInt32 unitId;
				spawnTable->tryGetString("name", spawn.name);

				if (!spawnTable->tryGetInteger("unit", unitId))
				{
					context.onError("Missing unit entry in creature spawn entry");
					return false;
				}

				spawn.unit = context.getUnit(unitId);
				if (!spawn.unit)
				{
					context.onError("Unknown unit in a creature spawn");
					return false;
				}

				spawn.emote = spawnTable->getInteger("emote", spawn.emote);
				spawn.radius = spawnTable->getInteger("radius", spawn.radius);
				spawn.respawn = (spawnTable->getInteger("respawn", static_cast<unsigned>(spawn.respawn)) != 0);
				spawn.respawnDelay = spawnTable->getInteger("respawnTime", spawn.respawnDelay);
				spawn.active = (spawnTable->getInteger("active", static_cast<unsigned>(spawn.active)) != 0);

				spawns.push_back(std::move(spawn));
			}
		}
		else
		{
			context.onError("Map is missing creature spawn array!");
			return false;
		}

		if (const sff::read::tree::Array<DataFileIterator> *const objectSpawnArray = wrapper.table.getArray("object_spawns"))
		{
			for (size_t j = 0, d = objectSpawnArray->getSize(); j < d; ++j)
			{
				const sff::read::tree::Table<DataFileIterator> *const spawnTable = objectSpawnArray->getTable(j);
				if (!spawnTable)
				{
					context.onError("Invalid spawn");
					return false;
				}

				ObjectSpawnPlacement spawn;

				{
					const sff::read::tree::Array<DataFileIterator> *const positionArray = spawnTable->getArray("position");
					if (!positionArray ||
					        !loadPosition(spawn.position, *positionArray))
					{
						context.onError("Invalid position in a spawn");
						return false;
					}

					spawnTable->tryGetInteger("orientation", spawn.orientation);

					const sff::read::tree::Array<DataFileIterator> *const rotationArray = spawnTable->getArray("rotation");
					if (rotationArray)
					{
						for (size_t i = 0, d = std::min(rotationArray->getSize(), spawn.rotation.size()); i < d; ++i)
						{
							spawn.rotation[i] = rotationArray->getInteger(i, 0.0f);
						}
					}
				}

				spawnTable->tryGetString("name", spawn.name);
				spawn.maxCount = spawnTable->getInteger("count", spawn.maxCount);

				UInt32 objectId;
				if (!spawnTable->tryGetInteger("object", objectId))
				{
					context.onError("Missing unit entry in object spawn entry");
					return false;
				}

				spawn.object = context.getObject(objectId);
				if (!spawn.object)
				{
					context.onError("Unknown object in an object spawn");
					return false;
				}

				spawn.radius = spawnTable->getInteger("radius", spawn.radius);
				spawn.respawn = (spawnTable->getInteger("respawn", static_cast<unsigned>(spawn.respawn)) != 0);
				spawn.respawnDelay = spawnTable->getInteger("respawnTime", spawn.respawnDelay);
				spawn.animProgress = spawnTable->getInteger("animProgress", spawn.animProgress);
				spawn.state = spawnTable->getInteger("state", spawn.state);

				objectSpawns.push_back(std::move(spawn));
			}
		}

		return true;
	}

	void MapEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) {
			context.table.addKey("name", name);
		}
		if (!directory.empty()) {
			context.table.addKey("directory", directory);
		}
		context.table.addKey("instanceType", constant_literal::mapInstanceType.getName(instanceType));

		{
			sff::write::Array<char> spawnArray(context.table, "creature_spawns", sff::write::MultiLine);

			for (auto &spawn : spawns)
			{
				sff::write::Table<char> spawnTable(spawnArray, sff::write::Comma);

				if (!name.empty()) {
					spawnTable.addKey("name", spawn.name);
				}
				spawnTable.addKey("unit", spawn.unit->id);

				{
					sff::write::Array<char> positionArray(spawnTable, "position", sff::write::Comma);
					positionArray.addElement(spawn.position[0]);
					positionArray.addElement(spawn.position[1]);
					positionArray.addElement(spawn.position[2]);
					positionArray.finish();
				}

				if (spawn.emote != 0) {
					spawnTable.addKey("emote", spawn.emote);
				}
				if (spawn.rotation)
				{
					spawnTable.addKey("rotation", spawn.rotation);
				}
				spawnTable.addKey("count", spawn.maxCount);
				spawnTable.addKey("radius", static_cast<unsigned>(spawn.radius));
				spawnTable.addKey("respawn", spawn.respawn ? 1 : 0);
				spawnTable.addKey("respawnTime", spawn.respawnDelay);
				spawnTable.addKey("active", spawn.active ? 1 : 0);

				spawnTable.finish();
			}

			spawnArray.finish();
		}

		{
			sff::write::Array<char> spawnArray(context.table, "object_spawns", sff::write::MultiLine);

			for (auto &spawn : objectSpawns)
			{
				sff::write::Table<char> spawnTable(spawnArray, sff::write::Comma);

				if (!spawn.name.empty()) {
					spawnTable.addKey("name", spawn.name);
				}
				spawnTable.addKey("object", spawn.object->id);

				{
					sff::write::Array<char> positionArray(spawnTable, "position", sff::write::Comma);
					positionArray.addElement(spawn.position[0]);
					positionArray.addElement(spawn.position[1]);
					positionArray.addElement(spawn.position[2]);
					positionArray.finish();
				}

				if (spawn.orientation)
				{
					spawnTable.addKey("orientation", spawn.orientation);
				}

				{
					sff::write::Array<char> rotationArray(spawnTable, "rotation", sff::write::Comma);
					rotationArray.addElement(spawn.rotation[0]);
					rotationArray.addElement(spawn.rotation[1]);
					rotationArray.addElement(spawn.rotation[2]);
					rotationArray.addElement(spawn.rotation[3]);
					rotationArray.finish();
				}

				spawnTable.addKey("count", spawn.maxCount);
				spawnTable.addKey("radius", static_cast<unsigned>(spawn.radius));
				spawnTable.addKey("respawn", spawn.respawn ? 1 : 0);
				spawnTable.addKey("respawnTime", spawn.respawnDelay);
				spawnTable.addKey("animProgress", spawn.animProgress);
				spawnTable.addKey("state", spawn.state);

				spawnTable.finish();
			}

			spawnArray.finish();
		}
	}
}
