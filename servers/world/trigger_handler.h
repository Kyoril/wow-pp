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

#pragma once

#include "shared/proto_data/triggers.pb.h"
#include "common/timer_queue.h"
#include "common/countdown.h"
#include "game/trigger_handler.h"

namespace wowpp
{
	class GameUnit;
	class PlayerManager;
	class WorldInstance;
	namespace proto
	{
		class Project;
	}

	/// 
	class TriggerHandler final : public game::ITriggerHandler
	{
	public:

		/// 
		explicit TriggerHandler(proto::Project &project, PlayerManager &playerManager, TimerQueue &timers);

		/// Fires a trigger event.
		virtual void executeTrigger(const proto::TriggerEntry &entry, game::TriggerContext context, UInt32 actionOffset = 0, bool ignoreProbability = false) override;

	private:

		Int32 getActionData(const proto::TriggerAction &action, UInt32 index) const;
		const String &getActionText(const proto::TriggerAction &action, UInt32 index) const;
		WorldInstance *getWorldInstance(GameObject *owner) const;
		GameObject *getActionTarget(const proto::TriggerAction &action, game::TriggerContext &context);
		bool playSoundEntry(UInt32 sound, GameObject *source);

		void handleTrigger(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSay(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleYell(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetWorldObjectState(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetSpawnState(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetRespawnState(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleCastSpell(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleMoveTo(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetCombatMovement(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleStopAutoAttack(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleCancelCast(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetStandState(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetVirtualEquipmentSlot(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetPhase(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetSpellCooldown(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleQuestKillCredit(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleQuestEventOrExploration(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetVariable(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleDismount(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleSetMount(const proto::TriggerAction &action, game::TriggerContext &context);
		void handleDespawn(const proto::TriggerAction &action, game::TriggerContext &context);

	private:

		bool checkInCombatFlag(const proto::TriggerEntry &entry, const GameObject *owner);
		bool checkOwnerAliveFlag(const proto::TriggerEntry &entry, const GameObject *owner);

	private:

		proto::Project &m_project;
		PlayerManager &m_playerManager;
		TimerQueue &m_timers;
		std::list<std::unique_ptr<Countdown>> m_delays;
	};
}
