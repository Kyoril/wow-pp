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
#include "trigger_handler.h"
#include "game/game_unit.h"
#include "proto_data/project.h"
#include "proto_data/trigger_helper.h"
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
#include "game/unit_mover.h"
#include "game/game_creature.h"
#include "common/make_unique.h"

namespace wowpp
{
	TriggerHandler::TriggerHandler(proto::Project &project, PlayerManager &playerManager, TimerQueue &timers)
		: m_project(project)
		, m_playerManager(playerManager)
		, m_timers(timers)
	{
	}

	void TriggerHandler::executeTrigger(const proto::TriggerEntry &entry, game::TriggerContext context, UInt32 actionOffset)
	{
		// Keep owner alive if provided
		std::shared_ptr<GameObject> strongOwner;
		if (context.owner) strongOwner = context.owner->shared_from_this();
		auto weakOwner = std::weak_ptr<GameObject>(strongOwner);

		// TODO: Find a better way to do this...
		// Remove all expired delays
		for (auto it = m_delays.begin(); it != m_delays.end();)
		{
			if (!(*it)->running)
			{
				it = m_delays.erase(it);
			}
			else
			{
				it++;
			}
		}

		if (static_cast<int>(actionOffset) >= entry.actions_size())
		{
			// Nothing to do here
			return;
		}

		for (int i = actionOffset; i < entry.actions_size(); ++i)
		{
			// Abort trigger on owner death?
			if (entry.flags() & trigger_flags::AbortOnOwnerDeath)
			{
				if (strongOwner)
				{
					if (strongOwner->isCreature() || strongOwner->isGameCharacter())
					{
						if (!std::static_pointer_cast<GameUnit>(strongOwner)->isAlive())
						{
							// Stop trigger execution here
							return;
						}
					}
				}
				else
				{
					// Stop trigger execution here
					return;
				}
			}

			const auto &action = entry.actions(i);
			switch (action.action())
			{
#define WOWPP_HANDLE_TRIGGER_ACTION(name) \
			case trigger_actions::name: \
				handle##name(action, context); \
				break;

			WOWPP_HANDLE_TRIGGER_ACTION(Trigger)
			WOWPP_HANDLE_TRIGGER_ACTION(Say)
			WOWPP_HANDLE_TRIGGER_ACTION(Yell)
			WOWPP_HANDLE_TRIGGER_ACTION(SetWorldObjectState)
			WOWPP_HANDLE_TRIGGER_ACTION(SetSpawnState)
			WOWPP_HANDLE_TRIGGER_ACTION(SetRespawnState)
			WOWPP_HANDLE_TRIGGER_ACTION(CastSpell)
			WOWPP_HANDLE_TRIGGER_ACTION(MoveTo)
			WOWPP_HANDLE_TRIGGER_ACTION(SetCombatMovement)
			WOWPP_HANDLE_TRIGGER_ACTION(StopAutoAttack)
			WOWPP_HANDLE_TRIGGER_ACTION(CancelCast)
			WOWPP_HANDLE_TRIGGER_ACTION(SetStandState)
			WOWPP_HANDLE_TRIGGER_ACTION(SetVirtualEquipmentSlot)
			WOWPP_HANDLE_TRIGGER_ACTION(SetPhase)
			WOWPP_HANDLE_TRIGGER_ACTION(SetSpellCooldown)
			WOWPP_HANDLE_TRIGGER_ACTION(QuestKillCredit)
			WOWPP_HANDLE_TRIGGER_ACTION(QuestEventOrExploration)
			WOWPP_HANDLE_TRIGGER_ACTION(SetVariable)
			WOWPP_HANDLE_TRIGGER_ACTION(Dismount)
			WOWPP_HANDLE_TRIGGER_ACTION(SetMount)
#undef WOWPP_HANDLE_TRIGGER_ACTION

				case trigger_actions::Delay:
				{
					UInt32 timeMS = getActionData(action, 0);
					if (timeMS == 0)
					{
						WLOG("Delay with 0 ms ignored");
						break;
					}

					if (i == entry.actions_size() - 1)
					{
						WLOG("Delay as last trigger action has no effect and is ignored");
						break;
					}

					// Save delay
					auto delayCountdown = make_unique<Countdown>(m_timers);
					delayCountdown->ended.connect([&entry, i, this, context, weakOwner]()
					{
						GameObject *oldOwner = context.owner;

						auto strongOwner = weakOwner.lock();
						if (context.owner != nullptr && strongOwner == nullptr)
						{
							WLOG("Owner no longer exists, so the executing trigger might fail.");
							oldOwner = nullptr;
						}

						executeTrigger(entry, context, i + 1);
					});
					delayCountdown->setEnd(getCurrentTime() + timeMS);
					m_delays.emplace_back(std::move(delayCountdown));

					// Skip the other actions for now
					i = entry.actions_size();
					break;
				}
				default:
				{
					WLOG("Unsupported trigger action: " << action.action());
					break;
				}
			}
		}
	}

	void TriggerHandler::handleTrigger(const proto::TriggerAction &action, game::TriggerContext &context)
	{
		if (action.target() != trigger_action_target::None)
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

		// Execute another trigger
		executeTrigger(*trigger, context, 0);
	}

	void TriggerHandler::handleSay(const proto::TriggerAction &action, game::TriggerContext &context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			WLOG("TRIGGER_ACTION_SAY: No target found, action will be ignored");
			return;
		}

		auto *world = getWorldInstance(target);
		if (!world)
		{
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature())
		{
			WLOG("TRIGGER_ACTION_SAY: Needs a unit target, but target is no unit - action ignored");
			return;
		}

		// Prepare packet data
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::OutgoingPacket packet(sink);
		game::server_write::messageChat(
			packet,
			game::chat_msg::MonsterSay,
			game::language::Universal,
			"",
			0,			// TODO: Make parameter dependant
			getActionText(action, 0), 
			reinterpret_cast<GameUnit*>(target)
			);

		// Send packet to all nearby watchers
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

		// Eventually play sound file
		if (action.data_size() > 0)
		{
			playSoundEntry(action.data(0), target);
		}
	}

	void TriggerHandler::handleYell(const proto::TriggerAction &action, game::TriggerContext &context)
	{
		GameObject *target = getActionTarget(action, context);
		if (!target)
		{
			WLOG("TRIGGER_ACTION_YELL: No target found, action will be ignored");
			return;
		}
		
		auto *world = getWorldInstance(target);
		if (!world)
		{
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature())
		{
			WLOG("TRIGGER_ACTION_YELL: Needs a unit target, but target is no unit - action ignored");
			return;
		}

		// Prepare packet
		std::vector<char> buffer;
		io::VectorSink sink(buffer);
		game::OutgoingPacket packet(sink);
		game::server_write::messageChat(
			packet, 
			game::chat_msg::MonsterYell, 
			game::language::Universal,
			"",
			0,			// TODO: Make parameter dependant
			getActionText(action, 0), 
			reinterpret_cast<GameUnit*>(target)
			);

		// Send packet to all nearby watchers
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

		// Eventually play sound file
		if (action.data_size() > 0)
		{
			playSoundEntry(action.data(0), target);
		}
	}

	void TriggerHandler::handleSetWorldObjectState(const proto::TriggerAction &action, game::TriggerContext &context)
	{
		GameObject * target = getActionTarget(action, context);
		if (!target ||
			!target->isWorldObject())
		{
			WLOG("TRIGGER_ACTION_SET_WORLD_OBJECT_STATE: Invalid target");
			return;
		}

		// Target is WorldObject checked already
		target->setUInt32Value(world_object_fields::State, getActionData(action, 0));
	}

	void TriggerHandler::handleSetSpawnState(const proto::TriggerAction &action, game::TriggerContext &context)
	{
		auto world = getWorldInstance(context.owner);
		if (!world)
		{
			return;
		}

		if ((action.target() != trigger_action_target::NamedCreature && action.target() != trigger_action_target::NamedWorldObject) ||
			action.targetname().empty())
		{
			WLOG("TRIGGER_ACTION_SET_SPAWN_STATE: Invalid target");
			return;
		}

		if (action.target() == trigger_action_target::NamedCreature)
		{
			auto * spawner = world->findCreatureSpawner(action.targetname());
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
			auto * spawner = world->findObjectSpawner(action.targetname());
			if (!spawner)
			{
				WLOG("TRIGGER_ACTION_SET_SPAWN_STATE: Could not find named object spawner");
				return;
			}

			//const bool isActive = (getActionData(action, 0) == 0 ? false : true);
			// TODO: De/activate object spawner
		}
	}

	void TriggerHandler::handleSetRespawnState(const proto::TriggerAction &action, game::TriggerContext &context)
	{
		auto world = getWorldInstance(context.owner);
		if (!world)
		{
			return;
		}

		if ((action.target() != trigger_action_target::NamedCreature && action.target() != trigger_action_target::NamedWorldObject) ||
			action.targetname().empty())
		{
			WLOG("TRIGGER_ACTION_SET_RESPAWN_STATE: Invalid target");
			return;
		}

		if (action.target() == trigger_action_target::NamedCreature)
		{
			auto * spawner = world->findCreatureSpawner(action.targetname());
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

	void TriggerHandler::handleCastSpell(const proto::TriggerAction &action, game::TriggerContext &context)
	{
		// Determine caster
		GameObject *caster = getActionTarget(action, context);
		if (!caster)
		{
			ELOG("TRIGGER_ACTION_CAST_SPELL: No valid target found");
			return;
		}
		if (!caster->isCreature() && !caster->isGameCharacter())
		{
			ELOG("TRIGGER_ACTION_CAST_SPELL: Caster has to be a unit");
			return;
		}

		// Resolve spell id
		const auto *spell = m_project.spells.getById(getActionData(action, 0));
		if (!spell)
		{
			ELOG("TRIGGER_ACTION_CAST_SPELL: Invalid spell index or spell not found");
			return;
		}

		// Determine spell target
		UInt32 dataTarget = getActionData(action, 1);
		GameObject *target = nullptr;
		switch (dataTarget)
		{
			case trigger_spell_cast_target::Caster:
				target = caster;
				break;
			case trigger_spell_cast_target::CurrentTarget:
				// Type of caster already checked above
				target = reinterpret_cast<GameUnit*>(caster)->getVictim();
				break;
			default:
				ELOG("TRIGGER_ACTION_CAST_SPELL: Invalid spell cast target value of " << dataTarget);
				return;
		}

		if (!target)
		{
			// Do a warning here because this could also happen when the caster simply has no target, which
			// is not always a bug, but could sometimes be intended behaviour (maybe we want to turn off this warning later).
			WLOG("TRIGGER_ACTION_CAST_SPELL: Could not find target");
			return;
		}

		// Create spell target map and cast that spell
		SpellTargetMap targetMap;
		if (target->isCreature() || target->isGameCharacter())
		{
			targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
			targetMap.m_unitTarget = target->getGuid();
		}
		else if (target->isWorldObject())
		{
			targetMap.m_targetMap = game::spell_cast_target_flags::Object;
			targetMap.m_goTarget = target->getGuid();
		}
		reinterpret_cast<GameUnit*>(caster)->castSpell(std::move(targetMap), spell->id(), { 0, 0, 0 }, spell->casttime());
	}

	void TriggerHandler::handleMoveTo(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_MOVE_TO: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature())
		{
			WLOG("TRIGGER_ACTION_MOVE_TO: Needs a creature target, but target is no unit - action ignored");
			return;
		}

		auto &mover = reinterpret_cast<GameUnit*>(target)->getMover();
		boost::signals2::connection reached = mover.targetReached.connect_extended([target](const boost::signals2::connection &conn) {
			// Raise reached target trigger
			reinterpret_cast<GameCreature*>(target)->raiseTrigger(trigger_event::OnReachedTriggeredTarget);

			// This slot shall be executed only once
			conn.disconnect();
		});

		mover.moveTo(
			math::Vector3(
				static_cast<float>(getActionData(action, 0)), 
				static_cast<float>(getActionData(action, 1)), 
				static_cast<float>(getActionData(action, 2))
			)
		);
	}

	void TriggerHandler::handleSetCombatMovement(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_SET_COMBAT_MOVEMENT: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature())
		{
			WLOG("TRIGGER_ACTION_SET_COMBAT_MOVEMENT: Needs a unit target, but target is no creature - action ignored");
			return;
		}

		// Update combat movement setting
		reinterpret_cast<GameCreature*>(target)->setCombatMovement(
			getActionData(action, 0) != 0);
	}

	void TriggerHandler::handleStopAutoAttack(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_STOP_AUTO_ATTACK: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature() && !target->isGameCharacter())
		{
			WLOG("TRIGGER_ACTION_STOP_AUTO_ATTACK: Needs a unit target - action ignored");
			return;
		}

		// Stop auto attack
		reinterpret_cast<GameUnit*>(target)->stopAttack();
	}

	void TriggerHandler::handleCancelCast(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_CANCEL_CAST: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature() && !target->isGameCharacter())
		{
			WLOG("TRIGGER_ACTION_CANCEL_CAST: Needs a unit target - action ignored");
			return;
		}

		reinterpret_cast<GameUnit*>(target)->cancelCast(game::spell_interrupt_flags::None);
	}

	void TriggerHandler::handleSetStandState(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_SET_STAND_STATE: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature() && !target->isGameCharacter())
		{
			WLOG("TRIGGER_ACTION_SET_STAND_STATE: Needs a unit target - action ignored");
			return;
		}

		Int32 data = getActionData(action, 0);
		if (data < 0 || data >= unit_stand_state::Count)
		{
			WLOG("TRIGGER_ACTION_SET_STAND_STATE: Invalid stand state (" << data << ")");
			return;
		}

		reinterpret_cast<GameUnit*>(target)->setStandState(static_cast<UnitStandState>(data));
	}

	void TriggerHandler::handleSetVirtualEquipmentSlot(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_SET_VIRTUAL_EQUIPMENT_SLOT: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature() && !target->isGameCharacter())
		{
			WLOG("TRIGGER_ACTION_SET_VIRTUAL_EQUIPMENT_SLOT: Needs a unit target - action ignored");
			return;
		}

		Int32 slot = getActionData(action, 0);
		if (slot < 0 || slot > 2)
		{
			ELOG("TRIGGER_ACTION_SET_VIRTUAL_EQUIPMENT_SLOT: Invalid equipment slot (" << slot << ")");
			return;
		}

		Int32 item = getActionData(action, 1);
		if (item > 0)
		{
			auto *itemEntry = m_project.items.getById(item);
			if (itemEntry)
			{
				reinterpret_cast<GameCreature*>(target)->setVirtualItem(slot, itemEntry);
			}
			else
			{
				ELOG("TRIGGER_ACTION_SET_VIRTUAL_EQUIPMENT_SLOT: Could not find item " << item);
			}
		}
		else
		{
			reinterpret_cast<GameCreature*>(target)->setVirtualItem(slot, nullptr);
		}
	}

	void TriggerHandler::handleSetPhase(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		WLOG("TODO: ACTION_SET_PHASE");
	}

	void TriggerHandler::handleSetSpellCooldown(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_SET_SPELL_COOLDOWN: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameUnit class
		if (!target->isCreature() && !target->isGameCharacter())
		{
			WLOG("TRIGGER_ACTION_SET_SPELL_COOLDOWN: Needs a unit target - action ignored");
			return;
		}

		reinterpret_cast<GameUnit*>(target)->setCooldown(getActionData(action, 0), getActionData(action, 1));
	}

	void TriggerHandler::handleQuestKillCredit(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_QUEST_KILL_CREDIT: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameCharacter class
		if (!target->isGameCharacter())
		{
			WLOG("TRIGGER_ACTION_QUEST_KILL_CREDIT: Needs a player target - action ignored");
			return;
		}

		UInt32 entryId = getActionData(action, 0);
		if (!entryId)
		{
			WLOG("TRIGGER_ACTION_QUEST_KILL_CREDIT: Needs a valid unit entry - action ignored");
			return;
		}

		const auto *entry = target->getProject().units.getById(entryId);
		if (!entry)
		{
			WLOG("TRIGGER_ACTION_QUEST_KILL_CREDIT: Unknown unit id " << entryId << " - action ignored");
			return;
		}

		if (!context.owner)
		{
			WLOG("TRIGGER_ACTION_QUEST_KILL_CREDIT: Unknown trigger owner (this is most likely due to a wrong assigned trigger! Assign it to a unit)");
			return;
		}

		reinterpret_cast<GameCharacter*>(target)->onQuestKillCredit(context.owner->getGuid(), *entry);
	}

	void TriggerHandler::handleQuestEventOrExploration(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_QUEST_EVENT_OR_EXPLORATION: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameCharacter class
		if (!target->isGameCharacter())
		{
			WLOG("TRIGGER_ACTION_QUEST_EVENT_OR_EXPLORATION: Needs a player target - action ignored");
			return;
		}

		UInt32 questId = getActionData(action, 0);
		reinterpret_cast<GameCharacter*>(target)->completeQuest(questId);
	}

	void TriggerHandler::handleSetVariable(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_SET_VARIABLE: No target found, action will be ignored");
			return;
		}

		// Get variable
		UInt32 entryId = getActionData(action, 0);
		if (entryId == 0)
		{
			WLOG("TRIGGER_ACTION_SET_VARIABLE: Needs a valid variable entry - action ignored");
			return;
		}

		const auto *entry = target->getProject().variables.getById(entryId);
		if (!entry)
		{
			WLOG("TRIGGER_ACTION_SET_VARIABLE: Unknown variable id " << entryId << " - action ignored");
			return;
		}

		// Determine variable type
		switch (entry->data_case())
		{
			// TODO
			case proto::VariableEntry::kIntvalue:
			case proto::VariableEntry::kLongvalue:
			case proto::VariableEntry::kFloatvalue:
			{
				target->setVariable(entryId, static_cast<Int64>(getActionData(action, 1)));
				break;
			}
			case proto::VariableEntry::kStringvalue:
			{
				target->setVariable(entryId, getActionText(action, 0));
				break;
			}
		}
	}

	void TriggerHandler::handleDismount(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_DISMOUNT: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameCharacter class
		if (!target->isGameCharacter() && !target->isCreature())
		{
			WLOG("TRIGGER_ACTION_DISMOUNT: Needs a unit target - action ignored");
			return;
		}

		target->setUInt32Value(unit_fields::MountDisplayId, 0);
	}

	void TriggerHandler::handleSetMount(const proto::TriggerAction & action, game::TriggerContext & context)
	{
		GameObject *target = getActionTarget(action, context);
		if (target == nullptr)
		{
			ELOG("TRIGGER_ACTION_SET_MOUNT: No target found, action will be ignored");
			return;
		}

		// Verify that "target" extends GameCharacter class
		if (!target->isGameCharacter() && !target->isCreature())
		{
			WLOG("TRIGGER_ACTION_SET_MOUNT: Needs a unit target - action ignored");
			return;
		}

		UInt32 mountId = getActionData(action, 0);
		if (!mountId)
		{
			WLOG("TRIGGER_ACTION_SET_MOUNT: Invalid mount id 0 used. If you want to dismount a unit, use the Dismount action!");
			return;
		}

		target->setUInt32Value(unit_fields::MountDisplayId, mountId);
	}

	Int32 TriggerHandler::getActionData(const proto::TriggerAction &action, UInt32 index) const
	{
		// Return default value (0) if not enough data provided
		if (static_cast<int>(index) >= action.data_size())
			return 0;

		return action.data(index);
	}

	const String & TriggerHandler::getActionText(const proto::TriggerAction &action, UInt32 index) const
	{
		// Return default string (empty) if not enough data provided
		if (static_cast<int>(index) >= action.texts_size())
		{
			static String invalidString = "<INVALID_TEXT>";
			return invalidString;
		}

		return action.texts(index);
	}

	WorldInstance * TriggerHandler::getWorldInstance(GameObject *owner) const
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

	bool TriggerHandler::playSoundEntry(UInt32 sound, GameObject * source)
	{
		if (sound)
		{
			auto *world = getWorldInstance(source);
			if (!world)
			{
				return false;
			}

			TileIndex2D srcTile;
			source->getTileIndex(srcTile);

			std::vector<char> soundBuffer;
			io::VectorSink soundSink(soundBuffer);
			game::OutgoingPacket soundPacket(soundSink);
			game::server_write::playSound(soundPacket, sound);
			forEachTileInRange(
				world->getGrid(),
				srcTile,
				5,
				[&soundBuffer, &soundPacket](VisibilityTile &tile)
			{
				for (auto *subscriber : tile.getWatchers())
				{
					subscriber->sendPacket(soundPacket, soundBuffer);
				}
			});
		}

		return true;
	}

	GameObject * TriggerHandler::getActionTarget(const proto::TriggerAction &action, game::TriggerContext &context)
	{
		switch (action.target())
		{
		case trigger_action_target::OwningObject:
			return context.owner;
		case trigger_action_target::SpellCaster:
			return context.spellCaster;
		case trigger_action_target::OwningUnitVictim:
			return (context.owner && context.owner->isCreature() ? reinterpret_cast<GameUnit*>(context.owner)->getVictim() : nullptr);
		case trigger_action_target::NamedWorldObject:
		{
			auto *world = getWorldInstance(context.owner);
			if (!world) return nullptr;

			// Need to provide a name
			if (action.targetname().empty()) return nullptr;

			// Find it
			auto *spawner = world->findObjectSpawner(action.targetname());
			if (!spawner) return nullptr;

			// Return the first spawned game object
			return (spawner->getSpawnedObjects().empty() ? nullptr : spawner->getSpawnedObjects()[0].get());
		}
		case trigger_action_target::NamedCreature:
		{
			auto *world = getWorldInstance(context.owner);
			if (!world) return nullptr;

			// Need to provide a name
			if (action.targetname().empty()) return nullptr;

			// Find it
			auto *spawner = world->findCreatureSpawner(action.targetname());
			if (!spawner) return nullptr;

			// Return the first spawned game object
			return (spawner->getCreatures().empty() ? nullptr : reinterpret_cast<GameObject*>(spawner->getCreatures()[0].get()));
		}
		default:
			WLOG("Unhandled action target " << action.target());
			break;
		}

		return nullptr;
	}
}
