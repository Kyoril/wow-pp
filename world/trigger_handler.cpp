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

#include "trigger_handler.h"
#include "game/game_unit.h"
#include "data/project.h"
#include "player_manager.h"
#include "game/visibility_grid.h"
#include "game/visibility_tile.h"
#include "game/each_tile_in_region.h"
#include "game/world_instance.h"
#include "game/tile_subscriber.h"
#include "game/world_object_spawner.h"
#include "game/game_world_object.h"
#include "log/default_log_levels.h"
#include "binary_io/vector_sink.h"
#include "binary_io/writer.h"
#include "game_protocol/game_protocol.h"

namespace wowpp
{
	TriggerHandler::TriggerHandler(Project &project, PlayerManager &playerManager)
		: m_project(project)
		, m_playerManager(playerManager)
	{
		// Iterate through all triggers and watch for their execution
		for (const auto &trigger : m_project.triggers.getTemplates())
		{
			trigger->execute.connect(
				std::bind(&TriggerHandler::executeTrigger, this, std::placeholders::_1, std::placeholders::_2));
		}
	}

	void TriggerHandler::executeTrigger(const TriggerEntry &entry, GameUnit *owner)
	{
		for (auto &action : entry.actions)
		{
			switch (action.action)
			{
#define WOWPP_HANDLE_TRIGGER_ACTION(name) \
			case trigger_actions::name: \
					handle##name(action, owner); \
					break;

				WOWPP_HANDLE_TRIGGER_ACTION(Trigger)
				WOWPP_HANDLE_TRIGGER_ACTION(Say)
				WOWPP_HANDLE_TRIGGER_ACTION(Yell)
				WOWPP_HANDLE_TRIGGER_ACTION(SetWorldObjectState)
				WOWPP_HANDLE_TRIGGER_ACTION(SetSpawnState)
				WOWPP_HANDLE_TRIGGER_ACTION(SetRespawnState)
				WOWPP_HANDLE_TRIGGER_ACTION(CastSpell)

#undef WOWPP_HANDLE_TRIGGER_ACTION

				default:
				{
					WLOG("Unsupported trigger action: " << action.action);
					break;
				}
			}
		}
	}

	void TriggerHandler::handleTrigger(const TriggerEntry::TriggerAction &action, GameUnit *owner)
	{
		if (action.target != trigger_action_target::None)
		{
			WLOG("Unsupported target provided for TRIGGER_ACTION_TRIGGER - has no effect");
		}

		UInt32 data = getActionData(action, 0);

		// Find that trigger
		const auto *trigger = m_project.triggers.getById(data);
		if (!trigger)
		{
			ELOG("Unable to find trigger " << data << " - trigger is not executed");
			return;
		}

		trigger->execute(*trigger, owner);
	}

	void TriggerHandler::handleSay(const TriggerEntry::TriggerAction &action, GameUnit *owner)
	{
		GameUnit *target = nullptr;
		switch (action.target)
		{
			case trigger_action_target::OwningUnit:
			{
				target = owner;
				break;
			}
			case trigger_action_target::OwningUnitVictim:
			{
				target = owner->getVictim();
				break;
			}
			default:
			{
				WLOG("TRIGGER_ACTION_SAY: Invalid target mode");
				return;
			}
		}

		if (target == nullptr)
		{
			WLOG("TRIGGER_ACTION_SAY: No target found, action will be ignored");
			return;
		}

		auto *world = target->getWorldInstance();
		if (!world)
		{
			WLOG("Target is not in a world instance right now - action will be ignored");
			return;
		}

		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::OutgoingPacket packet(sink);
		game::server_write::messageChat(packet, game::chat_msg::MonsterSay, game::language::Universal, "", 0, getActionText(action, 0), target);

		TileIndex2D tile;
		target->getTileIndex(tile);

		forEachTileInRange(
			world->getGrid(),
			tile,
			5,
			[&buffer, &packet](VisibilityTile &tile)
		{
			for (auto *subscriber : tile.getWatchers())
			{
				subscriber->sendPacket(packet, buffer);
			}
		});
	}

	void TriggerHandler::handleYell(const TriggerEntry::TriggerAction &action, GameUnit *owner)
	{
		// TODO: Better way to resolve targets
		GameUnit *target = nullptr;
		switch (action.target)
		{
			case trigger_action_target::OwningUnit:
			{
				target = owner;
				break;
			}
			case trigger_action_target::OwningUnitVictim:
			{
				target = owner->getVictim();
				break;
			}
			default:
			{
				WLOG("TRIGGER_ACTION_YELL: Invalid target mode");
				return;
			}
		}

		if (target == nullptr)
		{
			WLOG("TRIGGER_ACTION_YELL: No target found, action will be ignored");
			return;
		}
		
		auto *world = target->getWorldInstance();
		if (!world)
		{
			WLOG("Target is not in a world instance right now - action will be ignored");
			return;
		}

		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::OutgoingPacket packet(sink);
		game::server_write::messageChat(packet, game::chat_msg::MonsterYell, game::language::Universal, "", 0, getActionText(action, 0), target);

		TileIndex2D tile;
		target->getTileIndex(tile);

		forEachTileInRange(
			world->getGrid(),
			tile,
			5,
			[&buffer, &packet](VisibilityTile &tile)
		{
			for (auto *subscriber : tile.getWatchers())
			{
				subscriber->sendPacket(packet, buffer);
			}
		});

		if (action.data.size() > 0 &&
			action.data[0] > 0)
		{
			std::vector<char> soundBuffer;
			io::VectorSink soundSink(soundBuffer);
			game::OutgoingPacket soundPacket(soundSink);
			game::server_write::playSound(soundPacket, action.data[0]);

			forEachTileInRange(
				world->getGrid(),
				tile,
				5,
				[&soundBuffer, &soundPacket](VisibilityTile &tile)
			{
				for (auto *subscriber : tile.getWatchers())
				{
					subscriber->sendPacket(soundPacket, soundBuffer);
				}
			});
		}
	}

	void TriggerHandler::handleSetWorldObjectState(const TriggerEntry::TriggerAction &action, GameUnit *owner)
	{
		auto world = getWorldInstance(owner);
		if (!world)
		{
			return;
		}

		if (action.target != trigger_action_target::NamedWorldObject ||
			action.targetName.empty())
		{
			WLOG("TRIGGER_ACTION_SET_WORLD_OBJECT_STATE: Invalid target");
			return;
		}

		// Look for named object
		auto * spawner = world->findObjectSpawner(action.targetName);
		if (!spawner)
		{
			WLOG("TRIGGER_ACTION_SET_WORLD_OBJECT_STATE: Could not find named world object spawner");
			return;
		}

		const auto &spawned = spawner->getSpawnedObjects();
		if (spawned.empty())
		{
			WLOG("TRIGGER_ACTION_SET_WORLD_OBJECT_STATE: No objects spawned");
			return;
		}

		for (auto &spawn : spawned)
		{
			spawn->setUInt32Value(world_object_fields::State, getActionData(action, 0));
		}
	}

	void TriggerHandler::handleSetSpawnState(const TriggerEntry::TriggerAction &action, GameUnit *owner)
	{
		auto world = getWorldInstance(owner);
		if (!world)
		{
			return;
		}

		if ((action.target != trigger_action_target::NamedCreature && action.target != trigger_action_target::NamedWorldObject) ||
			action.targetName.empty())
		{
			WLOG("TRIGGER_ACTION_SET_SPAWN_STATE: Invalid target");
			return;
		}

		if (action.target == trigger_action_target::NamedCreature)
		{
			auto * spawner = world->findCreatureSpawner(action.targetName);
			if (!spawner)
			{
				WLOG("TRIGGER_ACTION_SET_SPAWN_STATE: Could not find named creature spawner");
				return;
			}

			const bool isActive = (getActionData(action, 0) == 0 ? false : true);
			spawner->setState(isActive);
		}
		else
		{
			DLOG("TODO");
		}
	}

	void TriggerHandler::handleSetRespawnState(const TriggerEntry::TriggerAction &action, GameUnit *owner)
	{
		auto world = getWorldInstance(owner);
		if (!world)
		{
			return;
		}

		if ((action.target != trigger_action_target::NamedCreature && action.target != trigger_action_target::NamedWorldObject) ||
			action.targetName.empty())
		{
			WLOG("TRIGGER_ACTION_SET_RESPAWN_STATE: Invalid target");
			return;
		}

		if (action.target == trigger_action_target::NamedCreature)
		{
			auto * spawner = world->findCreatureSpawner(action.targetName);
			if (!spawner)
			{
				WLOG("TRIGGER_ACTION_SET_RESPAWN_STATE: Could not find named creature spawner");
				return;
			}

			const bool isEnabled = (getActionData(action, 0) == 0 ? false : true);
			spawner->setRespawn(isEnabled);
		}
		else
		{
			DLOG("TODO");
		}
	}

	void TriggerHandler::handleCastSpell(const TriggerEntry::TriggerAction &action, GameUnit *owner)
	{
		GameUnit *target = getUnitTarget(action, owner);
		if (!target)
		{
			ELOG("TRIGGER_ACTION_CAST_SPELL: No valid target found");
			return;
		}

		const auto *spell = m_project.spells.getById(getActionData(action, 0));
		if (!spell)
		{
			ELOG("TRIGGER_ACTION_CAST_SPELL: Invalid spell index or spell not found");
			return;
		}

		SpellTargetMap targetMap;
		targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
		targetMap.m_unitTarget = target->getGuid();
		owner->castSpell(std::move(targetMap), spell->id, 0, [](game::SpellCastResult result)
		{
			DLOG("SPELL_CAST_RESULT: " << result);
		});
	}

	UInt32 TriggerHandler::getActionData(const TriggerEntry::TriggerAction &action, UInt32 index) const
	{
		// Return default value (0) if not enough data provided
		if (index >= action.data.size())
			return 0;

		return action.data[index];
	}

	const String & TriggerHandler::getActionText(const TriggerEntry::TriggerAction &action, UInt32 index) const
	{
		// Return default string (empty) if not enough data provided
		if (index >= action.texts.size())
		{
			static String invalidString = "<INVALID_TEXT>";
			return invalidString;
		}

		return action.texts[index];
	}

	WorldInstance * TriggerHandler::getWorldInstance(GameUnit *owner) const
	{
		WorldInstance *world = nullptr;
		if (owner)
		{
			world = owner->getWorldInstance();
		}

		if (!world)
		{
			ELOG("Could not get world instance - action will be ignored.");
		}

		return world;
	}

	GameUnit * TriggerHandler::getUnitTarget(const TriggerEntry::TriggerAction &action, GameUnit *owner)
	{
		switch (action.target)
		{
		case trigger_action_target::OwningUnit:
			return owner;
			break;
		case trigger_action_target::OwningUnitVictim:
			return (owner ? owner->getVictim() : nullptr);
			break;
		default:
			break;
		}

		return nullptr;
	}


}
