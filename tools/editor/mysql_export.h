#pragma once

#include "common/typedefs.h"
#include "mysql_transfer.h"

namespace wowpp
{
	namespace proto
	{
		class Project;
	}

	namespace editor
	{
		/// Generates an export task for area triggers.
		TransferTask exportAreaTriggers(const proto::Project& project);
		/// Generates an export task for emotes.
		TransferTask exportEmotes(const proto::Project& project);
		/// Generates an export task for faction templates.
		TransferTask exportFactionTemplates(const proto::Project& project);
		/// Generates an export task for factions.
		TransferTask exportFactions(const proto::Project& project);
		/// Generates an export task for fishing loot.
		TransferTask exportFishingLoot(const proto::Project& project);
		/// Generates an export task for units.
		TransferTask exportUnits(const proto::Project& project);
		/// Generates an export task for unit spells.
		TransferTask exportUnitSpells(const proto::Project& project);
		/// Generates an export task for unit quests.
		TransferTask exportUnitQuests(const proto::Project& project);
		/// Generates an export task for unit end quests.
		TransferTask exportUnitEndQuests(const proto::Project& project);
		/// Generates an export task for zones.
		TransferTask exportZones(const proto::Project& project);
	}
}