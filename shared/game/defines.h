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
#include "common/enum_strings.h"
#include <map>

namespace wowpp
{
	namespace game
	{
		namespace gender
		{
			enum Type
			{
				Male		= 0x00,
				Female		= 0x01,
				None		= 0x02,

				Max			= 0x02
			};
		}

		typedef gender::Type Gender;

		namespace race
		{
			enum Type
			{
				Human		= 0x01,
				Orc			= 0x02,
				Dwarf		= 0x03,
				NightElf	= 0x04,
				Undead		= 0x05,
				Tauren		= 0x06,
				Gnome		= 0x07,
				Troll		= 0x08,
				Goblin		= 0x09,
				BloodElf	= 0x0A,
				Draenei		= 0x0B,

				AllPlayable = ((1 << (Human - 1)) | (1 << (Orc - 1)) | (1 << (Dwarf - 1)) | (1 << (NightElf - 1)) | (1 << (Undead - 1)) | (1 << (Tauren - 1)) | (1 << (Gnome - 1)) | (1 << (Troll - 1)) | (1 << (BloodElf - 1)) | (1 << (Draenei - 1))),
				Alliance = ((1 << (Human - 1)) | (1 << (Dwarf - 1)) | (1 << (NightElf - 1)) | (1 << (Gnome - 1)) | (1 << (Draenei - 1))),
				Horde = ((1 << (Orc - 1)) | (1 << (Undead - 1)) | (1 << (Tauren - 1)) | (1 << (Troll - 1)) | (1 << (BloodElf - 1))),

				Max			= 0x0B
			};
		}

		typedef race::Type Race;

		namespace char_class
		{
			enum Type
			{
				Warrior		= 0x01,
				Paladin		= 0x02,
				Hunter		= 0x03,
				Rogue		= 0x04,
				Priest		= 0x05,
				Shaman		= 0x07,
				Mage		= 0x08,
				Warlock		= 0x09,
				Druid		= 0x0B,

				AllPlayable = ((1 << (Warrior - 1)) | (1 << (Paladin - 1)) | (1 << (Hunter - 1)) | (1 << (Rogue - 1)) | (1 << (Priest - 1)) | (1 << (Shaman - 1)) | (1 << (Mage - 1)) | (1 << (Warlock - 1)) | (1 << (Druid - 1))),
				AllCreature = ((1 << (Warrior - 1)) | (1 << (Paladin - 1)) | (1 << (Mage - 1))),
				WandUsers	= ((1 << (Priest - 1)) | (1 << (Mage - 1)) | (1 << (Warlock - 1))),

				Max			= 0x0C
			};
		}

		typedef char_class::Type CharClass;

		namespace chat_msg
		{
			enum Type
			{
				Addon				= 0xFFFFFFFF,
				System				= 0x00,
				Say					= 0x01,
				Party				= 0x02,
				Raid				= 0x03,
				Guild				= 0x04,
				Officer				= 0x05,
				Yell				= 0x06,
				Whisper				= 0x07,
				WhisperInform		= 0x08,
				Reply				= 0x09,
				Emote				= 0x0A,
				TextEmote			= 0x0B,
				MonsterSay			= 0x0C,
				MonsterParty		= 0x0D,
				MonsterYell			= 0x0E,
				MonsterWhisper		= 0x0F,
				MonsterEmote		= 0x10,
				Channel				= 0x11,
				ChannelJoin			= 0x12,
				ChannelLeave		= 0x13,
				ChannelList			= 0x14,
				ChannelNotice		= 0x15,
				ChannelNoticeUser	= 0x16,
				Afk					= 0x17,
				Dnd					= 0x18,
				Ignored				= 0x19,
				Skill				= 0x1A,
				Loot				= 0x1B,
				Money				= 0x1C,
				Opening				= 0x1D,
				TradeSkill			= 0x1E,
				PetInfo				= 0x1F,
				CombatMiscInfo		= 0x20,
				CombatXPGain		= 0x21,
				CombatHonorGain		= 0x22,
				CombatFactionChange	= 0x23,
				BGSystemNeutral		= 0x24,
				BGSystemAlliance	= 0x25,
				BGSystemHorde		= 0x26,
				RaidLeader			= 0x27,
				RaidWarning			= 0x28,
				RaidBossWhisper		= 0x29,
				RaidBossEmote		= 0x2A,
				Filtered			= 0x2B,
				Battleground		= 0x2C,
				BattlegroundLeader	= 0x2D,
				Restricted			= 0x2E
			};
		}

		typedef chat_msg::Type ChatMsg;

		namespace language
		{
			enum Type
			{
				Universal			= 0x00,
				Orcish				= 0x01,
				Darnassian			= 0x02,
				Taurahe				= 0x03,
				Dwarvish			= 0x04,
				Common				= 0x07,
				Demonic				= 0x08,
				Titan				= 0x09,
				Thalassian			= 0x0A,
				Draconic			= 0x0B,
				Kalimag				= 0x0C,
				Gnomish				= 0x0D,
				Troll				= 0x0E,
				GutterSpeak			= 0x21,
				Draenei				= 0x23,
				Zombie				= 0x24,
				GnomishBinary		= 0x25,
				GoblinBinary		= 0x26,
				Addon				= 0xFFFFFFFF,

				Count_				= 0x13
			};
		}

		typedef language::Type Language;

		namespace movement_flags
		{
			enum Type
			{
				None = 0x00000000,
				Forward = 0x00000001,
				Backward = 0x00000002,
				StrafeLeft = 0x00000004,
				StrafeRight = 0x00000008,
				TurnLeft = 0x00000010,
				TurnRight = 0x00000020,
				PitchUp = 0x00000040,
				PitchDown = 0x00000080,
				WalkMode = 0x00000100,
				OnTransport = 0x00000200,
				Levitating = 0x00000400,
				Root = 0x00000800,
				Falling = 0x00001000,
				FallingFar = 0x00004000,
				Swimming = 0x00200000,
				Ascending = 0x00400000,
				CanFly = 0x00800000,
				Flying = 0x01000000,
				Flying2 = 0x02000000,
				SplineElevation = 0x04000000,
				SplineEnabled = 0x08000000,
				WaterWalking = 0x10000000,
				SafeFall = 0x20000000,
				Hover = 0x40000000,

				Moving =
				Forward | Backward | StrafeLeft | StrafeRight | PitchUp | PitchDown |
				Falling | FallingFar | Ascending | Flying2 | SplineElevation,
				Turning =
				TurnLeft | TurnRight,
			};
		}

		typedef movement_flags::Type MovementFlags;

		namespace spline_flags
		{
			enum Type
			{
				None = 0x00000000,
				Jump = 0x00000008,
				WalkMode = 0x00000100,
				Flying = 0x00000200
			};
		}

		typedef spline_flags::Type SplineFlags;

		namespace friend_result
		{
			enum Type
			{
				/// Something went wrong in the database.
				DatabaseError		= 0x00,
				/// Friend list capacity reached.
				ListFull			= 0x01,
				/// Friend is online notification.
				Online				= 0x02,
				/// Friend is offline notification.
				Offline				= 0x03,
				/// Could not find player with that name to make him a friend.
				NotFound			= 0x04,
				/// Player was removed from the friend list.
				Removed				= 0x05,
				/// New player added to the friend list, who is online right now.
				AddedOnline			= 0x06,
				/// New player added to the friend list, who is offline right now.
				AddedOffline		= 0x07,
				/// That player is already in the friend list.
				AlreadyAdded		= 0x08,
				/// You can't add yourself to your friend list.
				Self				= 0x09,
				/// That player is an enemy and thus can't be added to the friend list.
				Enemy				= 0x0A,
				/// Ignore list capacity reached.
				IgnoreFull			= 0x0B,
				/// Can't add yourself to the ignore list.
				IgnoreSelf			= 0x0C,
				/// Player couldn't be found so we can't add him to the ignore list.
				IgnoreNotFound		= 0x0D,
				/// That player is already on the ignore list.
				IgnoreAlreadyAdded	= 0x0E,
				/// New player added to the ignore list.
				IgnoreAdded			= 0x0F,
				/// Player was removed from the ignore list.
				IgnoreRemoved		= 0x10,
				/// That name is ambiguous, type more of the player's server name.
				IgnoreAmbigous		= 0x11,
				/// TODO
				MuteFull			= 0x12,
				/// TODO
				MuteSelf			= 0x13,
				/// TODO
				MuteNotFound		= 0x14,
				/// TODO
				MuteAlreadyAdded	= 0x15,
				/// TODO
				MuteAdded			= 0x16,
				/// TODO
				MuteRemoved			= 0x17,
				/// That name is ambiguous, type more of the player's server name
				MuteAmbigous		= 0x18,
				/// No message at the client - unknown.
				Unknown_7			= 0x19,
				/// Unknown friend response from server.
				Uknown				= 0x1A
			};
		}

		typedef friend_result::Type FriendResult;

		namespace friend_status
		{
			enum Type
			{
				/// The target is offline.
				Offline		= 0x00,
				/// The target is online.
				Online		= 0x01,
				/// The target is flagged AFK (Away from keyboard).
				Afk			= 0x02,
				/// The target is flagged DND (Do not disturb).
				Dnd			= 0x04,
				/// The target is flagged for RAF (Refer a friend)
				Raf			= 0x08
			};
		}

		typedef friend_status::Type FriendStatus;

		namespace social_flag
		{
			enum Type
			{
				/// Target is a friend.
				Friend		= 0x01,
				/// Chat messages and invites from this target are ignored.
				Ignored		= 0x02,
				/// This target is muted for us in voice chat sessions.
				Muted		= 0x04
			};
		}

		typedef social_flag::Type SocialFlag;

		// TODO: Move this?
		struct SocialInfo final
		{
			FriendStatus status;
			UInt32 flags;
			UInt32 area;
			UInt32 level;
			UInt32 class_;
			String note;

			/// 
			SocialInfo()
				: status(friend_status::Offline)
				, flags(0)
				, area(0)
				, level(0)
				, class_(0)
			{
			}

			/// 
			explicit SocialInfo(UInt32 flags_, String note_)
				: status(friend_status::Offline)
				, flags(flags_)
				, area(0)
				, level(0)
				, class_(0)
				, note(std::move(note_))
			{
			}
		};

		typedef std::map<UInt64, SocialInfo> SocialInfoMap;

		namespace area_teams
		{
			enum Type
			{
				/// No favored team on this map (enables PvP on PvP-flagged realms).
				None		= 0x00,
				/// Alliance controlled lands.
				Alliance	= 0x02,
				/// Horde controlled lands.
				Horde		= 0x04
			};
		}

		typedef area_teams::Type AreaTeams;

		namespace area_flags
		{
			enum Type
			{
				/// TODO
				Snow			= 0x00000001,
				/// TODO
				SlaveCapital	= 0x00000008,
				/// TODO
				SlaveCapital2	= 0x00000020,
				/// TODO
				AllowDuels		= 0x00000040,
				/// TODO
				Arena			= 0x00000080,
				/// TODO
				Capital			= 0x00000100,
				/// TODO
				City			= 0x00000200,
				/// TODO
				Outland			= 0x00000400,
				/// TODO
				Sanctuary		= 0x00000800,
				/// TODO
				NeedFly			= 0x00001000,
				/// TODO
				Outland2		= 0x00004000,
				/// TODO
				PvP				= 0x00008000,
				/// TODO
				ArenaInstance	= 0x00010000,
				/// TODO
				LowLevel		= 0x00100000,
				/// TODO
				Inside			= 0x02000000,
				/// TODO
				Outside			= 0x04000000
			};
		}

		typedef area_flags::Type AreaFlags;

		namespace faction_template_flags
		{
			enum Type
			{
				/// This faction attacks players that are involved in PvP combat.
				ContestedGuard		= 0x00001000
			};
		}

		typedef faction_template_flags::Type FactionTemplateFlags;

		namespace faction_masks
		{
			enum Type
			{
				/// Non-aggressive creature.
				None			= 0x00,
				/// Any player.
				Player			= 0x01,
				/// Player or creature from alliance team.
				Alliance		= 0x02,
				/// Player or creature from horde team.
				Horde			= 0x04,
				/// Aggressive creature from monster team
				Monster			= 0x08
			};
		}

		typedef faction_masks::Type FactionMasks;

		namespace map_types
		{
			enum Type
			{
				/// Only one instance of this map exists, globally.
				Common			= 0x00,
				/// An instance PvE Dungeon.
				Dungeon			= 0x01,
				/// An instance PvE Dungeon for more than 5 players.
				Raid			= 0x02,
				/// An instanced PvP battleground.
				Battleground	= 0x03,
				/// An instanced PvP arena.
				Arena			= 0x04
			};
		}

		typedef map_types::Type MapTypes;

		namespace ability_learn_type
		{
			enum Type
			{
				/// TODO
				OnGetProfessionSkill	= 0x01,
				/// TODO
				OnGetRaceOrClassSkill	= 0x02
			};
		};

		typedef ability_learn_type::Type AbilityLearnType;

		namespace item_enchantment_type
		{
			enum Type
			{
				/// TODO
				None		= 0x00,
				/// TODO
				CombatSpell	= 0x01,
				/// TODO
				Damage		= 0x02,
				/// TODO
				EquipSkill	= 0x03,
				/// TODO
				Resistance	= 0x04,
				/// TODO
				Stat		= 0x05,
				/// TODO
				Totem		= 0x06
			};
		}

		typedef item_enchantment_type::Type ItemEnchantmentType;

		namespace item_enchantment_aura_id
		{
			enum Type
			{
				/// TODO
				Poison		= 26,
				/// TODO
				Normal		= 28,
				/// TODO
				Fire		= 32,
				/// TODO
				Frost		= 33,
				/// TODO
				Nature		= 81,
				/// TODO
				Shadow		= 107
			};
		}

		typedef item_enchantment_aura_id::Type ItemEnchantmentAuraID;

		namespace totem_category_type
		{
			enum Type
			{
				/// TODO
				Knife		= 0x01,
				/// TODO
				Totem		= 0x02,
				/// TODO
				Rod			= 0x03,
				/// TODO
				Pick		= 0x15,
				/// TODO
				Stone		= 0x16,
				/// TODO
				Hammer		= 0x17,
				/// TODO
				Spanner		= 0x18
			};
		}

		namespace spell_effects
		{
			enum Type
			{
				InstantKill				= 1,
				SchoolDamage			= 2,
				Dummy					= 3,
				PortalTeleport			= 4,
				TeleportUnits			= 5,
				ApplyAura				= 6,
				EnvironmentalDamage		= 7,
				PowerDrain				= 8,
				HealthLeech				= 9,
				Heal					= 10,
				Bind					= 11,
				Portal					= 12,
				RitualBase				= 13,
				RitualSpecialize		= 14,
				RitualActivatePortal	= 15,
				QuestComplete			= 16,
				WeaponDamageNoSchool	= 17,
				Resurrect				= 18,
				AddExtraAttacks			= 19,
				Dodge					= 20,
				Evade					= 21,
				Parry					= 22,
				Block					= 23,
				CreateItem				= 24,
				Weapon					= 25,
				Defense					= 26,
				PersistentAreaAura		= 27,
				Summon					= 28,
				Leap					= 29,
				Energize				= 30,
				WeaponPercentDamage		= 31,
				TriggerMissile			= 32,
				OpenLock				= 33,
				SummonChangeItem		= 34,
				ApplyAreaAuraParty		= 35,
				LearnSpell				= 36,
				SpellDefense			= 37,
				Dispel					= 38,
				Language				= 39,
				DualWield				= 40,
				Effect_41				= 41,
				Effect_42				= 42,
				TeleportUnitsFaceCaster	= 43,
				SkillStep				= 44,
				Effect_45				= 45,
				Spawn					= 46,
				TradeSkill				= 47,
				Stealth					= 48,
				Detect					= 49,
				TransDoor				= 50,
				ForceCriticalHit		= 51,
				GuaranteeHit			= 52,
				EnchantItem				= 53,
				EnchantItemTemporary	= 54,
				TameCreature			= 55,
				SummonPet				= 56,
				LearnPetSpell			= 57,
				WeaponDamage			= 58,
				OpenLockItem			= 59,
				Proficiency				= 60,
				SendEvent				= 61,
				PowerBurn				= 62,
				Threat					= 63,
				TriggerSpell			= 64,
				HealthFunnel			= 65,
				PowerFunnel				= 66,
				HealMaxHealth			= 67,
				InterruptCast			= 68,
				Distract				= 69,
				Pull					= 70,
				PickPocket				= 71,
				AddFarsight				= 72,
				Effect_73				= 73,
				Effect_74				= 74,
				HealMechanical			= 75,
				SummonObjectWild		= 76,
				ScriptEffect			= 77,
				Attack					= 78,
				Sanctuary				= 79,
				AddComboPoints			= 80,
				CreateHouse				= 81,
				BindSight				= 82,
				Duel					= 83,
				Stuck					= 84,
				SummonPlayer			= 85,
				ActivateObject			= 86,
				Effect_87				= 87,
				Effect_88				= 88,
				Effect_89				= 89,
				Effect_90				= 90,
				ThreatAll				= 91,
				EnchantHeldItem			= 92,
				Effect_93				= 93,
				SelfResurrect			= 94,
				Skinning				= 95,
				Charge					= 96,
				Effect_97				= 97,
				KnockBack				= 98,
				Disenchant				= 99,
				Inebriate				= 100,
				FeedPet					= 101,
				DismissPet				= 102,
				Reputation				= 103,
				Effect_104				= 104,
				Effect_105				= 105,
				Effect_106				= 106,
				Effect_107				= 107,
				DispelMechanic			= 108,
				SummonDeadPet			= 109,
				DestroyAllTotems		= 110,
				DurabilityDamage		= 111,
				ResurrectNew			= 113,
				AttackMe				= 114,
				DurabilityDamagePct		= 115,
				SkinPlayerCorpse		= 116,
				SpiritHeal				= 117,
				Skill					= 118,
				ApplyAreaAuraPet		= 119,
				TeleportGraveyard		= 120,
				NormalizedWeaponDmg		= 121,
				Effect_122				= 122,
				SendTaxi				= 123,
				PlayerPull				= 124,
				ModifyThreatPercent		= 125,
				StealBeneficialBuff		= 126,
				Prospecting				= 127,
				ApplyAreaAuraFriend		= 128,
				ApplyAreaAuraEnemy		= 129,
				RedirectThreat			= 130,
				Effect_131				= 131,
				PlayMusic				= 132,
				UnlearnSpecialization	= 133,
				KillCredit				= 134,
				CallPet					= 135,
				HealPct					= 136,
				EnergizePct				= 137,
				LeapBack				= 138,
				ClearQuest				= 139,
				ForceCast				= 140,
				Effect_141				= 141,
				TriggerSpellWithValue	= 142,
				ApplyAreaAuraOwner		= 143,
				KnockBack2				= 144,
				Effect_145				= 145,
				Effect_146				= 146,
				QuestFail				= 147,
				Effect_148				= 148,
				Effect_149				= 149,
				Effect_150				= 150,
				TriggerSpell2			= 151,
				SummonFriend			= 152,
				Effect_153				= 153,

				Count_					= 153,
				Invalid_				= 0
			};
		}

		typedef spell_effects::Type SpellEffect;

		namespace constant_literal
		{
			typedef EnumStrings < SpellEffect, spell_effects::Count_,
				spell_effects::Invalid_ > SpellEffectStrings;
			extern const SpellEffectStrings spellEffectNames;
		}
	}
}
