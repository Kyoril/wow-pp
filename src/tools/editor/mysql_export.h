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
		/// Generates an export task for maps.
		TransferTask exportMaps(const proto::Project& project);
		/// Generates an export task for map unit spawns.
		TransferTask exportMapUnitSpawns(const proto::Project& project);
		/// Generates an export task for map object spawns.
		TransferTask exportMapObjectSpawns(const proto::Project& project);
		/// Generates an export task for spells.
		TransferTask exportSpells(const proto::Project& project);
		/// Generates an export task for talents.
		TransferTask exportTalents(const proto::Project& project);
		/// Generates an export task for trainers.
		TransferTask exportTrainers(const proto::Project& project);
		/// Generates an export task for trainer spells.
		TransferTask exportTrainerSpells(const proto::Project& project);
		/// Generates an export task for triggers.
		TransferTask exportTriggers(const proto::Project& project);
		/// Generates an export task for trigger event data.
		TransferTask exportTriggerEvents(const proto::Project& project);
		/// Generates an export task for trigger event data.
		TransferTask exportTriggerActions(const proto::Project& project);
		/// Generates an export task for units.
		TransferTask exportUnits(const proto::Project& project);
		/// Generates an export task for unit spells.
		TransferTask exportUnitSpells(const proto::Project& project);
		/// Generates an export task for unit quests.
		TransferTask exportUnitQuests(const proto::Project& project);
		/// Generates an export task for unit end quests.
		TransferTask exportUnitEndQuests(const proto::Project& project);
		/// Generates an export task for vendors.
		TransferTask exportVendors(const proto::Project& project);
		/// Generates an export task for vendor items.
		TransferTask exportVendorItems(const proto::Project& project);
		/// Generates an export task for zones.
		TransferTask exportZones(const proto::Project& project);
	}
}