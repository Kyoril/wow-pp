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

#include "common/typedefs.h"
#include "common/enum_strings.h"
#include "common/vector.h"
#include <map>

namespace wowpp
{
	namespace game
	{
		typedef float Distance;
		typedef Vector<Distance, 2> Point;
		typedef Vector<Distance, 3> Position;

		inline Vector<Distance, 2> planar(const Position &point)
		{
			return Vector<Distance, 2>(
				point[0],
				point[1]);
		}

		namespace unit_stand_state
		{
			enum Type
			{
				Stand			= 0,
				Sit				= 1,
				SitChair		= 2,
				Sleep			= 3,
				SitLowChair		= 4,
				SitMediumChair	= 5,
				SitHighChair	= 6,
				Dead			= 7,
				Kneel			= 8
			};
		}

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
				Effect_112				= 112,
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

				Count_					= 154,
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

		namespace targets
		{
			enum Type
			{
				None					= 0,
				UnitCaster				= 1,
				UnitNearbyEnemy			= 2,
				UnitNearbyParty			= 3,
				UnitNearbyAlly			= 4,
				UnitPet					= 5,
				UnitTargetEnemy			= 6,
				UnitAreaEntrySrc		= 7,
				UnitAreaEntryDst		= 8,
				DstHome					= 9,
				UnitTargetDestCaster	= 11,
				UnitAreaEnemySrc		= 15,
				UnitAreaEnemyDst		= 16,
				DstDB					= 17,
				DstCaster				= 18,
				UnitPartyCaster			= 20,
				UnitTargetAlly			= 21,
				SrcCaster				= 22,
				GameObject				= 23,
				UnitConeEnemy			= 24,
				UnitTargetAny			= 25,
				GameObjectItem			= 26,
				UnitMaster				= 27,
				DestDynObjEnemy			= 28,
				DestDynObjAlly			= 29,
				UnitAreaAllySrc			= 30,
				UnitAreaAllyDst			= 31,
				Minion					= 32,
				AreaPartySrc			= 33,
				AreaPartyDst			= 34,
				UnitTargetParty			= 35,
				DestCasterRandomUnknown	= 36,
				UnitPartyTarget			= 37,
				UnitNearbyEntry			= 38,
				UnitCasterFishing		= 39,
				ObjectUse				= 40,
				DestCasterFrontLeft		= 41,
				DestCasterBackLeft		= 42,
				DestCasterBackRight		= 43,
				DestCasterFrontRight	= 44,
				UnitChainHeal			= 45,
				DstNearbyEntry			= 46,
				DestCasterFront			= 47,
				DestCasterBack			= 48,
				DestCasterRight			= 49,
				DestCasterLeft			= 50,
				ObjectAreaSrc			= 51,
				ObjectAreaDst			= 52,
				DstTargetEnemy			= 53,
				UnitConeEnemyUnknown	= 54,
				DestCasterFrontLeap		= 55,
				UnitRaidCaster			= 56,
				UnitRaidTargetRaid		= 57,
				UnitNearbyRaid			= 58,
				UnitConeAlly			= 59,
				UnitConeEntry			= 60,
				UnitClassTarget			= 61,
				Test					= 62,
				DestTargetAny			= 63,
				DestTargetFront			= 64,
				DestTargetBack			= 65,
				DestTargetRight			= 66,
				DestTargetLeft			= 67,
				DestTargetFrontLeft		= 68,
				DestTargetBackLeft		= 69,
				DestTargetBackRight		= 70,
				DestTargetFrontRight	= 71,
				DestCasterRandom		= 72,
				DestCasterRadius		= 73,
				DestTargetRandom		= 74,
				DestTargetRadius		= 75,
				DestChannel				= 76,
				UnitChannel				= 77,
				DestDestFront			= 78,
				DestDestBack			= 79,
				DestDestRight			= 80,
				DestDestLeft			= 81,
				DestDestFrontLeft		= 82,
				DestDestBackLeft		= 83,
				DestDestBackRight		= 84,
				DestDestFrontRight		= 85,
				DestDestRandom			= 86,
				DestDest				= 87,
				DestDynObjNone			= 88,
				DestTraj				= 89,
				UnitTargetMinipet		= 90,
				CorpseAreaEnemyPlayer	= 93
			};
		}

		typedef targets::Type Targets;

		namespace spell_cast_target_flags
		{
			enum Type
			{
				Self				= 0x00000000,
				Unit				= 0x00000002,
				Item				= 0x00000010,
				SourceLocation		= 0x00000020,
				DestLocation		= 0x00000040,
				ObjectUnknown		= 0x00000080,
				PvPCorpse			= 0x00000200,
				Object				= 0x00000800,
				TradeItem			= 0x00001000,
				String				= 0x00002000,
				Unk1				= 0x00004000,
				Corpse				= 0x00008000,
				Unk2				= 0x00010000
			};
		}

		typedef spell_cast_target_flags::Type SpellCastTargetFlags;

		namespace spell_miss_info
		{
			enum Type
			{
				None			= 0,
				Miss			= 1,
				Resist			= 2,
				Dodge			= 3,
				Parry			= 4,
				Block			= 5,
				Evade			= 6,
				Immune			= 7,
				Immune2			= 8,
				Deflect			= 9,
				Absorb			= 10,
				Reflect			= 11
			};
		}

		typedef spell_miss_info::Type SpellMissInfo;

		namespace spell_hit_type
		{
			enum Type
			{
				Unk1			= 0x00001,
				Crit			= 0x00002,
				Unk2			= 0x00004,
				Unk3			= 0x00008,
				Unk4			= 0x00020
			};
		}

		typedef spell_hit_type::Type SpellHitType;

		namespace spell_dmg_class
		{
			enum Type
			{
				None		= 0,
				Magic		= 1,
				Melee		= 2,
				Ranged		= 3
			};
		}

		typedef spell_dmg_class::Type SpellDmgClass;

		namespace spell_school
		{
			enum Type
			{
				Normal		= 0,
				Holy		= 1,
				Fire		= 2,
				Nature		= 3,
				Frost		= 4,
				Shadow		= 5,
				Arcane		= 6
			};
		}

		typedef spell_school::Type SpellSchool;

		namespace spell_school_mask
		{
			enum Type
			{
				None		= 0x00,
				Normal		= (1 << spell_school::Normal),
				Holy		= (1 << spell_school::Holy),
				Fire		= (1 << spell_school::Fire),
				Nature		= (1 << spell_school::Nature),
				Frost		= (1 << spell_school::Frost),
				Shadow		= (1 << spell_school::Shadow),
				Arcane		= (1 << spell_school::Arcane),
				Spell		= (Fire | Nature | Frost | Shadow | Arcane),
				Magic		= (Holy | Spell),
				All			= (Normal | Magic)
			};
		}

		typedef spell_school_mask::Type SpellSchoolMask;

		namespace spell_prevention_type
		{
			enum Type
			{
				None		= 0,
				Silence		= 1,
				Pacify		= 2
			};
		}

		typedef spell_prevention_type::Type SpellPreventionType;

		namespace spell_cast_result
		{
			enum Type
			{
				FailedAffectingCombat				= 0x00,
				FailedAlreadyAtFullHealth			= 0x01,
				FailedAlreadyAtFullMana				= 0x02,
				FailedAlreadyAtFullPower			= 0x03,
				FailedAlreadyBeingTamed				= 0x04,
				FailedAlreadyHaveCharm				= 0x05,
				FailedAlreadyHaveSummon				= 0x06,
				FailedAlreadyOpen					= 0x07,
				FailedAuraBounced					= 0x08,
				FailedAutotrackInterrupted			= 0x09,
				FailedBadImplicitTargets			= 0x0A,
				FailedBadTargets					= 0x0B,
				FailedCantBeCharmed					= 0x0C,
				FailedCantBeDisenchanted			= 0x0D,
				FailedCantBeDisenchantedSkill		= 0x0E,
				FailedCantBeProspected				= 0x0F,
				FailedCantCastOnTapped				= 0x10,
				FailedCantDuelWhileInvisible		= 0x11,
				FailedCantDuelWhileStealthed		= 0x12,
				FailedCantStealth					= 0x13,
				FailedCasterAurastate				= 0x14,
				FailedCasterDead					= 0x15,
				FailedCharmed						= 0x16,
				FailedChestInUse					= 0x17,
				FailedConfused						= 0x18,
				FailedDontReport					= 0x19,
				FailedEquippedItem					= 0x1A,
				FailedEquippedItemClass				= 0x1B,
				FailedEquippedItemClassMainhand		= 0x1C,
				FailedEquippedItemClassOffhand		= 0x1D,
				FailedError							= 0x1E,
				FailedFizzle						= 0x1F,
				FailedFleeing						= 0x20,
				FailedFoodLowLevel					= 0x21,
				FailedHighLevel						= 0x22,
				FailedHungerSatiated				= 0x23,
				FailedImmune						= 0x24,
				FailedInterrupted					= 0x25,
				FailedInterruptedCombat				= 0x26,
				FailedItemAlreadyEnchanted			= 0x27,
				FailedItemGone						= 0x28,
				FailedItemNotFound					= 0x29,
				FailedItemNotReady					= 0x2A,
				FailedLevelRequirement				= 0x2B,
				FailedLineOfSight					= 0x2C,
				FailedLowLevel						= 0x2D,
				FailedLowCastLevel					= 0x2E,
				FailedMainhandEmpty					= 0x2F,
				FailedMoving						= 0x30,
				FailedNeedAmmo						= 0x31,
				FailedNeedAmmoPouch					= 0x32,
				FailedNeedExoticAmmo				= 0x33,
				FailedNoPath						= 0x34,
				FailedNotBehind						= 0x35,
				FailedNotFishable					= 0x36,
				FailedNotFlying						= 0x37,
				FailedNotHere						= 0x38,
				FailedNotInfront					= 0x39,
				FailedNotInControl					= 0x3A,
				FailedNotKnown						= 0x3B,
				FailedNotMounted					= 0x3C,
				FailedNotOnTaxi						= 0x3D,
				FailedNotOnTransport				= 0x3E,
				FailedNotReady						= 0x3F,
				FailedNotShapeshift					= 0x40,
				FailedNotStanding					= 0x41,
				FailedNotTradeable					= 0x42,
				FailedNotTrading					= 0x43,
				FailedNotUnsheathed					= 0x44,
				FailedNotWhileGhost					= 0x45,
				FailedNoAmmo						= 0x46,
				FailedNoChargesRemain				= 0x47,
				FailedNoChampion					= 0x48,
				FailedNoComboPoints					= 0x49,
				FailedNoDueling						= 0x4A,
				FailedNoEndurance					= 0x4B,
				FailedNoFish						= 0x4C,
				FailedNoItemsWhileShapeshifted		= 0x4D,
				FailedNoMountsAllowed				= 0x4E,
				FailedNoPet							= 0x4F,
				FailedNoPower						= 0x50,
				FailedNothingToDispel				= 0x51,
				FailedNothingToSteal				= 0x52,
				FailedOnlyAboveWater				= 0x53,
				FailedOnlyDaytime					= 0x54,
				FailedOnlyIndoors					= 0x55,
				FailedOnlyMounted					= 0x56,
				FailedOnlyNighttime					= 0x57,
				FailedOnlyOutdoors					= 0x58,
				FailedOnlyShapeshift				= 0x59,
				FailedOnlyStealthed					= 0x5A,
				FailedOnlyUnderwater				= 0x5B,
				FailedOutOfRange					= 0x5C,
				FailedPacified						= 0x5D,
				FailedPossessed						= 0x5E,
				FailedReagents						= 0x5F,
				FailedRequiresArea					= 0x60,
				FailedRequiresSpellFocus			= 0x61,
				FailedRooted						= 0x62,
				FailedSilenced						= 0x63,
				FailedSpellInProgress				= 0x64,
				FailedSpellLearned					= 0x65,
				FailedSpellUnavailable				= 0x66,
				FailedStunned						= 0x67,
				FailedTargetsDead					= 0x68,
				FailedTargetAffectingCombat			= 0x69,
				FailedTargetAuraState				= 0x6A,
				FailedTargetDueling					= 0x6B,
				FailedTargetEnemy					= 0x6C,
				FailedTargetEnraged					= 0x6D,
				FailedTargetFriendly				= 0x6E,
				FailedTargetInCombat				= 0x6F,
				FailedTargetIsPlayer				= 0x70,
				FailedTargetIsPlayerControlled		= 0x71,
				FailedTargetNotDead					= 0x72,
				FailedTargetNotInParty				= 0x73,
				FailedTargetNotLooted				= 0x74,
				FailedTargetNotPlayer				= 0x75,
				FailedTargetNoPockets				= 0x76,
				FailedTargetNoWeapons				= 0x77,
				FailedTargetUnskinnable				= 0x78,
				FailedThirstSatiated				= 0x79,
				FailedTooClose						= 0x7A,
				FailedTooManyOfItem					= 0x7B,
				FailedTotemCategory					= 0x7C,
				FailedTotems						= 0x7D,
				FailedTrainingPoints				= 0x7E,
				FailedTryAgain						= 0x7F,
				FailedUnitNotBehind					= 0x80,
				FailedUnitNotInfront				= 0x81,
				FailedWrongPetFood					= 0x82,
				FailedNotWhileFatigued				= 0x83,
				FailedTargetNotInInstance			= 0x84,
				FailedNotWhileTrading				= 0x85,
				FailedTargetNotInRaid				= 0x86,
				FailedDisenchantWhileLooting		= 0x87,
				FailedProspectWhileLooting			= 0x88,
				FailedProspectNeedMore				= 0x89,
				FailedTargetFreeForAll				= 0x8A,
				FailedNoEdibleCorpses				= 0x8B,
				FailedOnlyBattlegrounds				= 0x8C,
				FailedTargetNotGhost				= 0x8D,
				FailedTooManySkills					= 0x8E,
				FailedTransformUnusable				= 0x8F,
				FailedWrongWeather					= 0x90,
				FailedDamageImmune					= 0x91,
				FailedPreventedByMechanic			= 0x92,
				FailedPlayTime						= 0x93,
				FailedReputation					= 0x94,
				FailedMinSkill						= 0x95,
				FailedNotInArena					= 0x96,
				FailedNotOnShapeshift				= 0x97,
				FailedNotOnStealthed				= 0x98,
				FailedNotOnDamageImmune				= 0x99,
				FailedNotOnMounted					= 0x9A,
				FailedTooShallow					= 0x9B,
				FailedTargetNotInSanctuary			= 0x9C,
				FailedTargetIsTrivial				= 0x9D,
				FailedBMOrInvisGod					= 0x9E,
				FailedExpertRidingRequirement		= 0x9F,
				FailedArtisanRidingRequirement		= 0xA0,
				FailedNotIdle						= 0xA1,
				FailedNotInactive					= 0xA2,
				FailedPartialPlaytime				= 0xA3,
				FailedNoPlaytime					= 0xA4,
				FailedNotInBattleground				= 0xA5,
				FailedOnlyInArena					= 0xA6,
				FailedTargetLockedToRaidInstance	= 0xA7,
				FailedUnknown						= 0xA8,
				/// Custom values used if no error occurred (will not be sent to the client)
				CastOkay							= 0xFF
			};
		}

		typedef spell_cast_result::Type SpellCastResult;

		namespace spell_range_index
		{
			enum Type
			{
				SelfOnly	= 1,
				Combat		= 2,
				Anywhere	= 13
			};
		}

		typedef spell_range_index::Type SpellRangeIndex;

		namespace spell_cast_flags
		{
			enum Type
			{
				Pending			= 0x00000001,
				Unknown1		= 0x00000002,
				Unknown2		= 0x00000010,
				Ammo			= 0x00000020,
				Unknown3		= 0x00000100
			};
		}

		typedef spell_cast_flags::Type SpellCastFlags;

		namespace spell_range_flag
		{
			enum Type
			{
				Default			= 0,
				Melee			= 1,
				Ranged			= 2
			};
		}

		typedef spell_range_flag::Type SpellRangeFlag;

		namespace skill_category
		{
			enum Type
			{
				Attributes		= 5,
				Weapon			= 6,
				Class			= 7,
				Armor			= 8,
				Secondary		= 9,
				Languages		= 10,
				Profession		= 11,
				NotDisplayed	= 12
			};
		}

		typedef skill_category::Type SkillCategory;

		namespace victim_state
		{
			enum Type
			{
				Unknown1	= 0,
				Normal		= 1,
				Dodge		= 2,
				Parry		= 3,
				Interrupt	= 4,
				Blocks		= 5,
				Evades		= 6,
				IsImmune	= 7,
				Deflects	= 8
			};
		}
		
		typedef victim_state::Type VictimState;

		namespace hit_info
		{
			enum Type
			{
				NormalSwing		= 0x00000000,
				Unknown1		= 0x00000001,
				NormalSwing2	= 0x00000002,
				LeftSwing		= 0x00000004,
				Miss			= 0x00000010,
				Absorb			= 0x00000020,
				Resist			= 0x00000040,
				CriticalHit		= 0x00000080,
				Unknown2		= 0x00000100,
				Unknown3		= 0x00000200,
				Glancing		= 0x00004000,
				Crushing		= 0x00008000,
				NoAction		= 0x00010000,
				SingNoHitSound	= 0x00080000
			};
		}

		typedef hit_info::Type HitInfo;

		namespace weapon_attack
		{
			enum Type
			{
				BaseAttack		= 0,
				OffhandAttack	= 1,
				RangedAttack	= 2
			};
		}

		typedef weapon_attack::Type WeaponAttack;

		namespace inventory_change_failure
		{
			enum Enum
			{
				Okay = 0,
				CantEquipLevel = 1,
				CantEquipSkill = 2,
				ItemDoesNotGoToSlot = 3,
				BagFull = 4,
				NonEmptyBagOverOtherBag = 5,
				CantTradeEquipBags = 6,
				OnlyAmmoCanGoHere = 7,
				NoRequiredProficiency = 8,
				NoEquipmentSlotAvailable = 9,
				YouCanNeverUseThatItem = 10,
				YouCanNeverUseThatItem2 = 11,
				NoEquipmentSlotAvailable2 = 12,
				CantEquipWithTwoHanded = 13,
				CantDualWield = 14,
				ItemDoesntGoIntoBag = 15,
				ItemDoesntGoIntoBag2 = 16,
				CantCarryMoreOfThis = 17,
				NoEquipmentSlotAvailable3 = 18,
				ItemCantStack = 19,
				ItemCantBeEquipped = 20,
				ItemsCantBeSwapped = 21,
				SlotIsEmpty = 22,
				ItemNotFound = 23,
				CantDropSoulbound = 24,
				OutOfRange = 25,
				TriedToSplitMoreThanCount = 26,
				CouldntSplitItems = 27,
				MissingReagent = 28,
				NotEnoughMoney = 29,
				NotABag = 30,
				CanOnlyDoWithEmptyBags = 31,
				DontOwnThatItem = 32,
				CanEquipOnlyOneQuiver = 33,
				MustPurchaseThatBagSlot = 34,
				TooFarAwayFromBank = 35,
				ItemLocked = 36,
				YouAreStunned = 37,
				YouAreDead = 38,
				CantDoRightNow = 39,
				InternalBagError = 40,
				CanEquipOnlyOneQuiver2 = 41,
				CanEquipOnlyOneAmmoPouch = 42,
				StackableCantBeWrapped = 43,
				EquippedCantBeWrapped = 44,
				WrappedCantBeWrapped = 45,
				BoundCantBeWrapped = 46,
				UniqueCantBeWrapped = 47,
				BagsCantBeWrapped = 48,
				AlreadyLooted = 49,
				InventoryFull = 50,
				BankFull = 51,
				ItemIsCurrentlySoldOut = 52,
				BagFull3 = 53,
				ItemNotFound2 = 54,
				ItemCantStack2 = 55,
				BagFull4 = 56,
				ItemSoldOut = 57,
				ObjectIsBusy = 58,
				None = 59,
				NotInCombat = 60,
				NotWhileDisarmed = 61,
				BagFull6 = 62,
				CantEquipBank = 63,
				CantEquipReputation = 64,
				TooManySpecialBags = 65,
				CantLootThatNow = 66,
				ItemUniqueEquippable = 67,
				VendorMissingTurnIns = 68,
				NotEnoughHonorPoints = 69,
				NotEnoughArenaPoints = 70,
				ItemMaxCountSocketed = 71,
				MailBoundItem = 72,
				NoSplitWhileProspecting = 73,
				ItemMaxCountEquippedSocketed = 75,
				ItemUniqueEquippableSocketed = 76,
				TooMuchGold = 77,
				NotDuringArenaMatch = 78,
				CannotTradeThat = 79,
				PersonalArenaRatingTooLow = 80
			};
		}

		typedef inventory_change_failure::Enum InventoryChangeFailure;

		namespace aura_type
		{
			enum Type
			{
				None								= 0,
				BindSight							= 1,
				ModPossess							= 2,
				PeriodicDamage						= 3,
				Dummy								= 4,
				ModConfuse							= 5,
				ModCharm							= 6,
				ModFear								= 7,
				PeriodicHeal						= 8,
				ModAttackSpeed						= 9,
				ModThreat							= 10,
				ModTaunt							= 11,
				ModStun								= 12,
				ModDamageDone						= 13,
				ModDamageTaken						= 14,
				DamageShield						= 15,
				ModStealth							= 16,
				ModStealthDetect					= 17,
				ModInvisibility						= 18,
				ModInvisibilityDetection			= 19,
				ObsModHealth						= 20,
				ObsModMana							= 21,
				ModResistance						= 22,
				PeriodicTriggerSpell				= 23,
				PeriodicEnergize					= 24,
				ModPacify							= 25,
				ModRoot								= 26,
				ModSilence							= 27,
				ReflectSpells						= 28,
				ModStat								= 29,
				ModSkill							= 30,
				ModIncreaseSpeed					= 31,
				ModIncreaseMountedSpeed				= 32,
				ModDecreaseSpeed					= 33,
				ModIncreaseHealth					= 34,
				ModIncreaseEnergy					= 35,
				ModShapeShift						= 36,
				EffectImmunity						= 37,
				StateImmunity						= 38,
				SchoolImmunity						= 39,
				DamageImmunity						= 40,
				DispelImmunity						= 41,
				ProcTriggerSpell					= 42,
				ProcTriggerDamage					= 43,
				TrackCreatures						= 44,
				TrackResources						= 45,
				ModParrySkill						= 46,
				ModParryPercent						= 47,
				ModDodgeSkill						= 48,
				ModDodgePercent						= 49,
				ModBlockSkill						= 50,
				ModBlockPercent						= 51,
				ModCritPercent						= 52,
				PeriodicLeech						= 53,
				ModHitChance						= 54,
				ModSpellHitChance					= 55,
				Transform							= 56,
				ModSpellCritChance					= 57,
				ModIncreaseSwimSpeed				= 58,
				ModDamageDoneCreature				= 59,
				ModPacifySilence					= 60,
				ModScale							= 61,
				PeriodicHealthFunnel				= 62,
				PeriodicManaFunnel					= 63,
				PeriodicManaLeech					= 64,
				ModCastingSpeed						= 65,
				FeignDeath							= 66,
				ModDisarm							= 67,
				ModStalked							= 68,
				SchoolAbsorb						= 69,
				ExtraAttacks						= 70,
				ModSpellCritChanceSchool			= 71,
				ModPowerCostSchoolPct				= 72,
				ModPowerCostSchool					= 73,
				ReflectSpellsSchool					= 74,
				ModLanguage							= 75,
				FarSight							= 76,
				MechanicImmunity					= 77,
				Mounted								= 78,
				ModDamagePercentDone				= 79,
				ModPercentStat						= 80,
				SplitDamagePct						= 81,
				WaterBreathing						= 82,
				ModBaseResistance					= 83,
				ModRegen							= 84,
				ModPowerRegen						= 85,
				ChannelDeathItem					= 86,
				ModDamagePercentTaken				= 87,
				ModHealthRegenPercent				= 88,
				PeriodicDamagePercent				= 89,
				ModResistChance						= 90,
				ModDetectRange						= 91,
				PreventsFleeing						= 92,
				ModUnattackable						= 93,
				InterruptRegen						= 94,
				Ghost								= 95,
				SpellMagnet							= 96,
				ManaShield							= 97,
				ModSkillTalent						= 98,
				ModAttackPower						= 99,
				AurasVisible						= 100,
				ModResistancePct					= 101,
				ModMeleeAttackPowerVersus			= 102,
				ModTotalThreat						= 103,
				WaterWalk							= 104,
				FeatherFall							= 105,
				Hover								= 106,
				AddFlatModifier						= 107,
				AddPctModifier						= 108,
				AddTargetTrigger					= 109,
				ModPowerRegenPercent				= 110,
				AddCasterHitTrigger					= 111,
				OverrideClassScripts				= 112,
				ModRangedDamageTaken				= 113,
				ModRangedDamageTakenPct				= 114,
				ModHealing							= 115,
				ModRegenDurationCombat				= 116,
				ModMechanicResistance				= 117,
				ModHealingPct						= 118,
				SharePetTracking					= 119,
				Untrackable							= 120,
				Empathy								= 121,
				ModOffhandDamagePct					= 122,
				ModTargetResistance					= 123,
				ModRangedAttackPower				= 124,
				ModMeleeDamageTaken					= 125,
				ModMeleeDamageTakenPct				= 126,
				RangedAttackPowerAttackerBonus		= 127,
				ModPossessPet						= 128,
				ModSpeedAlways						= 129,
				ModMOuntedSpeedAlways				= 130,
				ModRangedAttackPowerVersus			= 131,
				ModIncreaseEnergyPercent			= 132,
				ModIncreaseHealthPercent			= 133,
				ModManaRegenInterrupt				= 134,
				ModHealingDone						= 135,
				ModHealingDonePct					= 136,
				ModTotalStatPercentage				= 137,
				ModHaste							= 138,
				ForceReaction						= 139,
				ModRangedHaste						= 140,
				ModRangedAmmoHaste					= 141,
				ModBaseResistancePct				= 142,
				ModResistanceExclusive				= 143,
				SafeFall							= 144,
				Charisma							= 145,
				Persuaded							= 146,
				MechanicImmunityMask				= 147,
				RetainComboPoints					= 148,
				ResistPushback						= 149,
				ModShieldBlockValuePct				= 150,
				TrackStealthed						= 151,
				ModDetectedRange					= 152,
				SplitDamageFlat						= 153,
				ModStealthLevel						= 154,
				ModWaterBreathing					= 155,
				ModReputationGain					= 156,
				PetDamageMulti						= 157,
				ModShieldBlockValue					= 158,
				NoPvPCredit							= 159,
				ModAOEAvoidance						= 160,
				ModHealthRegenInCombat				= 161,
				PowerBurnMana						= 162,
				ModCritDamageBonusMelee				= 163,
				AuraType_164						= 164,
				MeleeAttackPowerAttackerBonus		= 165,
				ModattackPowerPct					= 166,
				ModRangedAttackPowerPct				= 167,
				ModDamageDoneVersus					= 168,
				ModCritPercentVersus				= 169,
				DetectAmore							= 170,
				ModSpeedNotStack					= 171,
				ModMountedSpeedNotStack				= 172,
				AllowChampionSpells					= 173,
				ModSpellDamageOfStatPercent			= 174,
				ModSpellhealingOfStatPercent		= 175,
				SpiritOfRedemption					= 176,
				AOECharm							= 177,
				ModDebuffResistance					= 178,
				ModAttackerSpellCritChance			= 179,
				ModFlatSpellDamageVersus			= 180,
				ModFlatSpellCritDamageVersus		= 181,
				ModResistanceOfStatPercent			= 182,
				ModCriticalThreat					= 183,
				ModAttackerMeleeHitChance			= 184,
				ModAttackerRangedHitChance			= 185,
				ModAttackerSpellHitChance			= 186,
				ModAttackerMeleeCritChance			= 187,
				ModAttackerRangedCritChance			= 188,
				ModRating							= 189,
				ModFactionReputationGain			= 190,
				UseNormalMovementSpeed				= 191,
				HasteMelee							= 192,
				MeleeSlow							= 193,
				ModDeprecated_194					= 194,
				ModDeprecated_195					= 195,
				ModCooldown							= 196,
				ModAttackerSpellAndWeaponCritChance	= 197,
				ModAllWeaponSkills					= 198,
				ModIncreaseSpellPctToHit			= 199,
				ModXpPct							= 200,
				Fly									= 201,
				IgnoreCombatResult					= 202,
				ModAttackerMeleeCritDamage			= 203,
				ModAttackerRangedCritDamage			= 204,
				AuraType_205						= 205,
				ModFlightSpeed						= 206,
				ModFlightSpeedMounted				= 207,
				ModFlightSpeedStacking				= 208,
				ModFlightSpeedMountedStacking		= 209,
				ModFlightSpeedNotStacking			= 210,
				ModFlightSpeedMountedNotStacking	= 211,
				ModRangedAttackPowerOfStatPercent	= 212,
				ModRageFromDamageDealt				= 213,
				AuraType_214						= 214,
				ArenaPreparation					= 215,
				HasteSpells							= 216,
				AuraType_217						= 217,
				HasteRanged							= 218,
				ModManaRegenFromStat				= 219,
				ModRatingFromStat					= 220,
				AuraType_221						= 221,
				AuraType_222						= 222,
				AuraType_223						= 223,
				AuraType_224						= 224,
				PrayerOfMending						= 225,
				PeriodicDummy						= 226,
				PeriodicTriggerSpellWithValue		= 227,
				DetectStealth						= 228,
				ModAOEDamageAvoidance				= 229,
				AuraType_230						= 230,
				ProcTriggerSpellWithValue			= 231,
				MechanicDurationMod					= 232,
				AuraType_233						= 233,
				MechanicDurationModNotStack			= 234,
				ModDispelResist						= 235,
				AuraType_236						= 236,
				ModSpellDamageOfAttackPower			= 237,
				ModSpellHealingOfAttackPower		= 238,
				ModScale_2							= 239,
				ModExpertise						= 240,
				ModForceMoveForward					= 241,
				ModSpellDamageFromHealing			= 242,
				AuraType_243						= 243,
				ComprehendLanguage					= 244,
				ModDurationOfMagicEffects			= 245,
				AuraType_246						= 246,
				AuraType_247						= 247,
				ModCombatResultChance				= 248,
				AuraType_249						= 249,
				ModIncreaseHealth_2					= 250,
				ModEnemyDodge						= 251,
				ReusedBlessedLife					= 252,
				ReusedIncreasePetOutdoorSpeed		= 253,
				AuraType_254						= 254,
				AuraType_255						= 255,
				AuraType_256						= 256,
				AuraType_257						= 257,
				AuraType_258						= 258,
				AuraType_259						= 259,
				AuraType_260						= 260,
				AuraType_261						= 261,

				Count_								= 262,
				Invalid_							= Count_
			};
		}

		typedef aura_type::Type AuraType;

		namespace constant_literal
		{
			typedef EnumStrings < AuraType, aura_type::Count_,
				aura_type::Invalid_ > AuraTypeStrings;
			extern const AuraTypeStrings auraTypeNames;
		}

		namespace area_aura_type
		{
			enum Type
			{
				Party		= 0,
				Friend		= 1,
				Enemy		= 2,
				Pet			= 3,
				Owner		= 4
			};
		}

		typedef area_aura_type::Type AreaAuraType;

		namespace lock_key_type
		{
			enum Type
			{
				/// No key requirement.
				None	= 0,
				/// An item is required.
				Item	= 1,
				/// A specific skill is required.
				Skill	= 2
			};
		}

		namespace lock_type
		{
			enum Type
			{
				/// 
				Picklock				= 1,
				/// 
				Herbalism				= 2,
				/// 
				Mining					= 3,
				/// 
				DisarmTrap				= 4,
				/// 
				Open					= 5,
				/// 
				Treasure				= 6,
				/// 
				CalcifiedElvenGems		= 7,
				/// 
				Close					= 8,
				/// 
				ArmTrap					= 9,
				/// 
				QuickOpen				= 10,
				/// 
				QuickClose				= 11,
				/// 
				OpenTinkering			= 12,
				/// 
				OpenKneeling			= 13,
				/// 
				OpenAttacking			= 14,
				/// 
				Gahzridian				= 15,
				/// 
				Blasting				= 16,
				/// 
				SlowOpen				= 17,
				/// 
				SlowClose				= 18,
				/// 
				Fishing					= 19
			};
		}

		namespace inventory_type
		{
			enum Type
			{
				NonEquip		= 0,
				Head			= 1,
				Neck			= 2,
				Shoulders		= 3,
				Body			= 4,
				Chest			= 5,
				Waist			= 6,
				Legs			= 7,
				Feet			= 8,
				Wrists			= 9,
				Hands			= 10,
				Finger			= 11,
				Trinket			= 12,
				Weapon			= 13,
				Shield			= 14,
				Ranged			= 15,
				Cloak			= 16,
				TwoHandedWeapon	= 17,
				Bag				= 18,
				Tabard			= 19,
				Robe			= 20,
				MainHandWeapon	= 21,
				OffHandWeapon	= 22,
				Holdable		= 23,
				Ammo			= 24,
				Thrown			= 25,
				RangedRight		= 26,
				Quiver			= 27,
				Relic			= 28
			};
		}

		namespace unit_flags
		{
			enum Type
			{
				/// 
				Unknown_0			= 0x00000001,
				/// Unit can't be attackaed.
				NotAttackable		= 0x00000002,
				/// 
				DisableMovement		= 0x00000004,
				/// Unit has pvp mode enabled, which will flag players for pvp if they attack or support this unit, too.
				PvPMode				= 0x00000008,
				/// 
				Rename				= 0x00000010,
				/// Doesn't take reagents for spells with attribute ex 5 "NoReatentWhileRep"
				Preparation			= 0x00000020,
				/// 
				Unknown_1			= 0x00000040,
				/// Must be compined with PvPMode flags. Makes this unit unattackable from pvp targets.
				NotAttackablePvP	= 0x00000080,
				/// Unit is not attackable while it is not in combat.
				OOCNotAttackable	= 0x00000100,
				/// Makes the unit non-aggressive, even when hostile to other units - until it's attackaed directly.
				Passive				= 0x00000200,
				/// Shows the looting animation.
				Looting				= 0x00000400,
				/// 
				PetInCombat			= 0x00000800,
				/// 
				PvP					= 0x00001000,
				/// 
				Silenced			= 0x00002000,
				/// 
				Unknown_2			= 0x00004000,
				/// 
				Unknown_3			= 0x00008000,
				/// Can't be targeted by a spell cast directly.
				NoSpellTarget		= 0x00010000,
				/// 
				Pacified			= 0x00020000,
				/// 
				Stunned				= 0x00040000,
				///
				InCombat			= 0x00080000,
				/// Disables casting at client side for spells which aren't allowed during flight.
				TaxiFlight			= 0x00100000,
				/// Disables melee spell casting.
				Disarmed			= 0x00200000,
				/// 
				Confused			= 0x00400000,
				/// 
				Fleeing				= 0x00800000,
				/// Used in spell Eye of the Beast for pets.
				PlayerControlled	= 0x01000000,
				/// 
				NotSelectable		= 0x02000000,
				/// 
				Skinnable			= 0x04000000,
				/// 
				Mount				= 0x08000000,
				/// 
				Unknown_4			= 0x10000000,
				Unknown_5			= 0x20000000,
				Unknown_6			= 0x40000000,
				Sheathe				= 0x80000000,
			};
		}
	
		namespace faction_flags
		{
			enum Type
			{
				/// Makes visible in client (set or can be set at interaction with target of this faction)
				Visible			= 0x01,
				/// Enable AtWar button in client. Player controlled (except opposition team always war state). Flag only set on initial creation.
				AtWar			= 0x02,
				/// Hidden faction from reputation pane in client (Player can gain reputation, but this update is not sent to the client)
				Hidden			= 0x04,
				/// Used to hide opposite team factions.
				InvisibleForced	= 0x08,
				/// Used to prevent war with own team factions.
				PeaceForced		= 0x10,
				/// Player controlled
				Inactive		= 0x20,
				/// Flag for the two competing Outland factions
				Rival			= 0x40
			};
		}

		namespace reputation_rank
		{
			enum Type
			{
				Hated			= 0,
				Hostile			= 1,
				Unfriendly		= 2,
				Neutral			= 3,
				Friendly		= 4,
				Honored			= 5,
				Revered			= 6,
				Exalted			= 7
			};
		}

		typedef reputation_rank::Type ReputationRank;
	}
}
