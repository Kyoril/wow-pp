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
#include "game_incoming_packet.h"
#include "game_outgoing_packet.h"
#include "binary_io/reader.h"
#include "common/sha1.h"
#include "common/vector.h"
#include "game/defines.h"
#include "data/unit_entry.h"
#include "game/action_button.h"
#include "game/game_unit.h"
#include "game/movement_info.h"
#include "game/spell_target_map.h"
#include "data/spell_entry.h"
#include "data/item_entry.h"
#include "data/object_entry.h"
#include "data/vendor_entry.h"
#include <array>
#include <vector>
#include <functional>

namespace wowpp
{
	class GameItem;
	class GameCharacter;
	class LootInstance;

	namespace game
	{
		struct Protocol
		{
			typedef game::IncomingPacket IncomingPacket;
			typedef game::OutgoingPacket OutgoingPacket;
		};

		/// Enumerates all OP codes sent by the client.
		namespace client_packet
		{
			enum
			{
				CharCreate				= 0x036,
				CharEnum				= 0x037,
				CharDelete				= 0x038,
				PlayerLogin				= 0x03D,
				PlayerLogout			= 0x04A,
				LogoutRequest			= 0x04B,
				LogoutCancel			= 0x04E,
				NameQuery				= 0x050,
				ItemQuerySingle			= 0x056,
				ItemQueryMultiple		= 0x057,
				GameObjectQuery			= 0x05E,
				CreatureQuery			= 0x060,
				ContactList				= 0x066,
				AddFriend				= 0x069,
				DeleteFriend			= 0x06A,
				SetContactNotes			= 0x06B,
				AddIgnore				= 0x06C,
				DeleteIgnore			= 0x06D,
				GroupInvite				= 0x06E,
				GroupAccept				= 0x072,
				GroupDecline			= 0x073,
				GroupUninvite			= 0x075,
				GroupUninviteGUID		= 0x076,
				GroupSetLeader			= 0x078,
				LootMethod				= 0x07A,
				GroupDisband			= 0x07B,
				MessageChat				= 0x095,
				UseItem					= 0x0AB,
				AreaTrigger				= 0x0B4,
				MoveStartForward		= 0x0B5,
				MoveStartBackward		= 0x0B6,
				MoveStop				= 0x0B7,
				StartStrafeLeft			= 0x0B8,
				StartStrafeRight		= 0x0B9,
				StopStrafe				= 0x0BA,
				Jump					= 0x0BB,
				StartTurnLeft			= 0x0BC,
				StartTurnRight			= 0x0BD,
				StopTurn				= 0x0BE,
				StartPitchUp			= 0x0BF,
				StartPitchDown			= 0x0C0,
				StopPitch				= 0x0C1,
				MoveSetRunMode			= 0x0C2,
				MoveSetWalkMode			= 0x0C3,
				MoveFallLand			= 0x0C9,
				MoveStartSwim			= 0x0CA,
				MoveStopSwim			= 0x0CB,
				SetFacing				= 0x0DA,
				SetPitch				= 0x0DB,
				MoveWorldPortAck		= 0x0DC,
				MoveHeartBeat			= 0x0EE,
				ForceMoveRootAck		= 0x0E9,
				ForceMoveUnrootAck		= 0x0EB,
				CompleteCinematic		= 0x0FC,
				TutorialFlag			= 0x0FE,
				TutorialClear			= 0x0FF,
				TutorialReset			= 0x100,
				StandStateChange		= 0x101,
				Emote					= 0x102,
				TextEmote				= 0x104,
				AutoStoreLootItem		= 0x108,
				AutoEquipItem			= 0x10A,
				AutoStoreBagItem		= 0x10B,
				SwapItem				= 0x10C,
				SwapInvItem				= 0x10D,
				SplitItem				= 0x10E,
				AutoEquipItemSlot		= 0x10F,
				DestroyItem				= 0x111,
				SetActionButton			= 0x128,
				CastSpell				= 0x12E,
				CancelCast				= 0x12F,
				CancelAura				= 0x136,
				SetSelection			= 0x13D,
				AttackSwing				= 0x141,
				AttackStop				= 0x142,
				RepopRequest			= 0x15A,
				Loot					= 0x15D,
				LootMoney				= 0x15E,
				LootRelease				= 0x15F,
				ListInventory			= 0x19E,
				SellItem				= 0x1A0,
				BuyItem					= 0x1A2,
				BuyItemInSlot			= 0x1A3,
				Ping					= 0x1DC,
				SetSheathed				= 0x1E0,
				AuthSession				= 0x1ED,
				LearnTalent				= 0x251,
				TogglePvP				= 0x253,
				RequestPartyMemberStats	= 0x27F,
				GroupRaidConvert		= 0x28E,
				GroupAssistentLeader	= 0x28F,
				MoveFallReset			= 0x2CA,
				RaidTargetUpdate		= 0x321,
				RaidReadyCheck			= 0x322,
				SetDungeonDifficulty	= 0x329,
				MoveSetFly				= 0x346,
				MoveStartAscend			= 0x359,
				MoveStopAscend			= 0x35A,
				MoveChangeTransport		= 0x38D,
				TimeSyncResponse		= 0x391,
				MoveStartDescend		= 0x3A7,
				RaidReadyCheckFinished	= 0x3C5
			};
		}

		namespace transfer_abort_reason
		{
			enum Type
			{
				None				= 0x0000,
				/// Transfer Aborted: instance is full
				MaxPlayers			= 0x0001,
				/// Transfer aborted: instance not found
				NotFound			= 0x0002,
				/// You have entered too many instances recently.
				TooManyInstances	= 0x0003,
				/// Unable to zone in while an encounter is in progress.
				ZoneInCombat		= 0x0005,
				/// You must have TBC expansion installed to access this area.
				InsufExpanLevel1	= 0x0106,
				/// Normal difficulty mode is not available for %s.
				Difficulty1			= 0x0007,
				/// Heroic difficulty mode is not available for %s.
				Difficulty2			= 0x0107,
				/// Epic difficulty mode is not available for %s.
				Difficulty3			= 0x0207
			};
		}

		typedef transfer_abort_reason::Type TransferAbortReason;

		namespace session_status
		{
			enum Type
			{
				Never			= 0x00,
				Always			= 0x01,
				Connected		= 0x02,
				Authentificated	= 0x03,
				LoggedIn		= 0x04,
				TransferPending	= 0x05
			};
		}

		typedef session_status::Type SessionStatus;

		namespace group_member_status
		{
			enum Type
			{
				Offline = 0x0000,
				Online = 0x0001,
				PvP = 0x0002,
				Dead = 0x0004,
				Ghost = 0x0008,
				Unknown_1 = 0x0040,
				ReferAFriendBuddy = 0x0100
			};
		}

		typedef group_member_status::Type GroupMemberStatus;

		/// Enumerates possible OP codes which the server can send to the client.
		/// OP codes taken from the MaNGOS project: http://www.getmangos.eu
		namespace server_packet
		{
			enum
			{
				TriggerCinematic			= 0x0FA,
				CharCreate					= 0x03A,
				CharEnum					= 0x03B,
				CharDelete					= 0x03C,
				NewWorld					= 0x03E,
				TransferPending				= 0x03F,
				TransferAborted				= 0x040,
				CharacterLoginFailed		= 0x041,
				LoginSetTimeSpeed			= 0x042,
				LogoutResponse				= 0x04C,
				LogoutComplete				= 0x04D,
				LogoutCancelAck				= 0x04F,
				NameQueryResponse			= 0x051,
				ItemQuerySingleResponse		= 0x058,
				ItemQueryMultipleResponse	= 0x059,
				GameObjectQueryResponse		= 0x05F,
				CreatureQueryResponse		= 0x061,
				ContactList					= 0x067,
				FriendStatus				= 0x068,
				GroupInvite					= 0x06F,
				GroupCancel					= 0x071,
				GroupDecline				= 0x074,
				GroupUninvite				= 0x077,
				GroupSetLeader				= 0x079,
				GroupDestroyed				= 0x07C,
				GroupList					= 0x07D,
				PartyMemberStats			= 0x07E,
				PartyCommandResult			= 0x07F,
				MessageChat					= 0x096,
				UpdateObject				= 0x0A9,
				DestroyObject				= 0x0AA,
				MoveTeleportAck				= 0x0C7,
				MonsterMove					= 0x0DD,
				ForceMoveRoot				= 0x0E8,
				ForceMoveUnroot				= 0x0EA,
				MoveRoot					= 0x0EC,
				MoveUnroot					= 0x0ED,
				MoveHeartBeat				= 0x0EE,
				TutorialFlags				= 0x0FD,
				Emote						= 0x103,
				TextEmote					= 0x105,
				InventoryChangeFailure		= 0x112,
				InitializeFactions			= 0x122,
				SetProficiency				= 0x127,
				ActionButtons				= 0x129,
				InitialSpells				= 0x12A,
				LearnedSpell				= 0x12B,
				CastFailed					= 0x130,
				SpellStart					= 0x131,
				SpellGo						= 0x132,
				SpellFailure				= 0x133,
				SpellCooldown				= 0x134,
				CooldownEvent				= 0x135,
				UpdateAuraDuration			= 0x137,
				AiReaction					= 0x13C,
				AttackStart					= 0x143,
				AttackStop					= 0x144,
				AttackSwingNotInRange		= 0x145,
				AttackSwingBadFacing		= 0x146,
				AttackSwingNotStanding		= 0x147,
				AttackSwingDeadTarget		= 0x148,
				AttackSwingCantAttack		= 0x149,
				AttackerStateUpdate			= 0x14A,
				SpellHealLog				= 0x150,
				SpellEnergizeLog			= 0x151,
				BindPointUpdate				= 0x155,
				LootResponse				= 0x160,
				LootReleaseResponse			= 0x161,
				LootRemoved					= 0x162,
				LootMoneyNotify				= 0x163,
				LootItemNotify				= 0x164,
				LootClearMoney				= 0x165,
				ItemPushResult				= 0x166,
				ListInventory				= 0x19F,
				LogXPGain					= 0x1D0,
				Pong						= 0x1DD,
				LevelUpInfo					= 0x1D4,
				AuthChallenge				= 0x1EC,
				AuthResponse				= 0x1EE,
				CompressedUpdateObject		= 0x1F6,
				ExplorationExperience		= 0x1F8,
				EnvironmentalDamageLog		= 0x1FC,
				AccountDataTimes			= 0x209,
				ChatWrongFaction			= 0x219,
				SetRestStart				= 0x21E,
				LoginVerifyWorld			= 0x236,
				PeriodicAuraLog				= 0x24E,
				SpellDamageShield			= 0x24F,
				SpellNonMeleeDamageLog		= 0x250,
				StandStateUpdate			= 0x29D,
				SpellFailedOther			= 0x2A6,
				ChatPlayerNotFound			= 0x2A9,
				DurabilityDamageDeath		= 0x2BD,
				InitWorldStates				= 0x2C2,
				PlaySound					= 0x2D2,
				AddonInfo					= 0x2EF,
				PartyMemberStatsFull		= 0x2F2,
				RaidTargetUpdate			= 0x321,
				RaidReadyCheck				= 0x322,
				SetDungeonDifficulty		= 0x329,
				Motd						= 0x33D,
				TimeSyncReq					= 0x390,
				UpdateComboPoints			= 0x39D,
				SetExtraAuraInfo			= 0x3A4,
				SetExtraAuraInfoNeedUpdate	= 0x3A5,
				RaidReadyCheckConfirm		= 0x3AE,
				RaidReadyCheckFinished		= 0x3C5,
				FeatureSystemStatus			= 0x3C8,
				UnlearnSpells				= 0x41D,	
			};
		}

		struct AddonEntry
		{
			String addonNames;
			UInt8 unk6;
			UInt32 crc, unk7;
		};

		typedef std::vector<AddonEntry> AddonEntries;

		struct CharEntry
		{
			DatabaseId id;
			String name;
			Race race;
			CharClass class_;
			Gender gender;
			UInt8 level;
			UInt8 skin;
			UInt8 face;
			UInt8 hairStyle;
			UInt8 hairColor;
			UInt8 facialHair;
			UInt8 outfitId;
			UInt32 mapId;
			UInt32 zoneId;
			float x, y, z;
			float o;
			bool cinematic;
			std::map<UInt8, const ItemEntry*> equipment;

			explicit CharEntry()
				: id(0)
				, race(race::Human)
				, class_(char_class::Warrior)
				, gender(gender::Male)
				, level(1)
				, skin(0)
				, face(0)
				, hairStyle(0)
				, hairColor(0)
				, facialHair(0)
				, outfitId(0)
				, mapId(0)
				, zoneId(0)
				, x(0.0f)
				, y(0.0f)
				, z(0.0f)
				, o(0.0f)
				, cinematic(false)
			{
			}
		};

		typedef std::vector<CharEntry> CharEntries;

		namespace response_code
		{
			enum Type
			{
				Success							= 0x00,
				Failure							= 0x01,
				Cancelled						= 0x02,
				Disconnected					= 0x03,
				FailedToConnect					= 0x04,
				Connected						= 0x05,
				VersionMismatch					= 0x06,

				AuthOk							= 0x0C,
				AuthFailed						= 0x0D,
				AuthReject						= 0x0E,
				AuthBadServerProof				= 0x0F,
				AuthUnavailable					= 0x10,
				AuthSystemError					= 0x11,
				AuthBillingError				= 0x12,
				AuthBillingExpired				= 0x13,
				AuthVersionMismatch				= 0x14,
				AuthUnknownAccount				= 0x15,
				AuthIncorrectPassword			= 0x16,
				AuthSessionExpired				= 0x17,
				AuthServerShuttingDown			= 0x18,
				AuthAlreadyLoggedIn				= 0x19,
				AuthLoginServerNotFound			= 0x1A,
				AuthWaitQueue					= 0x1B,
				AuthBanned						= 0x1C,
				AuthAlreadyOnline				= 0x1D,
				AuthNoTime						= 0x1E,
				AuthDbBusy						= 0x1F,
				AuthSuspended					= 0x20,
				AuthParentalControl				= 0x21,

				CharCreateInProgress			= 0x2E,
				CharCreateSuccess				= 0x2F,
				CharCreateError					= 0x30,
				CharCreateFailed				= 0x31,
				CharCreateNameInUse				= 0x32,
				CharCreateDisabled				= 0x33,
				CharCreatePvPTeamsViolation		= 0x34,
				CharCreateServerLimit			= 0x35,
				CharCreateAccountLimit			= 0x36,
				CharCreateServerQueue			= 0x37,
				CharCreateOnlyExisting			= 0x38,
				CharCreateExpansion				= 0x39,

				CharDeleteInProgress			= 0x3A,
				CharDeleteSuccess				= 0x3B,
				CharDeleteFailed				= 0x3C,
				CharDeleteFailedLockedForTransfer = 0x3D,
				CharDeleteFailedGuildLeader		= 0x3E,
				CharDeleteFailedArenaCaptain	= 0x3F,

				CharLoginInProgress				= 0x40,
				CharLoginSuccess				= 0x00,
				CharLoginNoWorld				= 0x01,
				CharLoginDuplicateCharacter		= 0x02,
				CharLoginNoInstances			= 0x03,
				CharLoginFailed					= 0x04,
				CharLoginDisabled				= 0x05,
				CharLoginNoCharacter			= 0x06,
				CharLoginLockedForTransfer		= 0x07,
				CharLoginLockedByBilling		= 0x08
			};
		}

		typedef response_code::Type ResponseCode;

		namespace atlogin_flags
		{
			enum Type
			{
				/// Nothing special happens at login.
				None			= 0x00,
				/// Player will be forced to rename his character before entering the world.
				Rename			= 0x01,
				/// Character spellbook will be reset (unlearn all learned spells).
				ResetSpells		= 0x02,
				/// Character talents will be reset.
				ResetTalents	= 0x04,
				/// Indicates that this character never logged in before.
				FirstLogin		= 0x20,
			};
		}

		typedef atlogin_flags::Type AtLoginFlags;

		namespace expansions
		{
			enum Type
			{
				/// Vanilla wow (1.0 - 1.12.X)
				Classic = 0x00,
				/// The Burning Crusade (2.0 - 2.4.3)
				TheBurningCrusade = 0x01
			};
		}

		typedef expansions::Type Expansions;

		namespace party_operation
		{
			enum Type
			{
				Invite		= 0,
				Leave		= 2
			};
		}

		typedef party_operation::Type PartyOperation;

		namespace party_result
		{
			enum Type
			{
				/// Silent, no client output.
				Ok						= 0,
				/// Cannot find player "%s".
				CantFindTarget			= 1,
				/// %s is not in your party.
				NotInYourParty			= 2,
				/// %s is not in your instance.
				NotInYourInstance		= 3,
				/// Your party is full.
				PartyFull				= 4,
				/// %s is already in a group.
				AlreadyInGroup			= 5,
				/// You aren't in a party.
				YouNotInGroup			= 6,
				/// You are not the party leader.
				YouNotLeader			= 7,
				/// Target is unfriendly.
				TargetUnfriendly		= 8,
				/// %s is ignoring you.
				TargetIgnoreYou			= 9,
				/// You already have a pending match.
				PendingMatch			= 12,
				/// Trial accounts cannot invite characters into groups.
				InviteRestricted		= 13
			};
		}

		typedef party_result::Type PartyResult;

		/// Stores data of a group member.
		struct GroupMemberSlot final
		{
			/// Group member name.
			String name;
			/// Group id (0-7)
			UInt8 group;
			/// Is assistant?
			bool assistant;
			/// Status
			UInt32 status;

			/// 
			GroupMemberSlot()
				: group(0)
				, assistant(false)
				, status(0)
			{
			}
		};

		namespace client_read
		{
			bool ping(
				io::Reader &packet,
				UInt32 &out_ping,
				UInt32 &out_latency
				);

			bool authSession(
				io::Reader &packet,
				UInt32 &out_clientBuild,
				String &out_Account,
				UInt32 &out_clientSeed,
				SHA1Hash &out_digest,
				AddonEntries &out_addons
				);

			bool charEnum(
				io::Reader &packet
				);

			bool charCreate(
				io::Reader &packet,
				CharEntry &out_entry
				);

			bool charDelete(
				io::Reader &packet,
				DatabaseId &out_characterId
				);

			bool nameQuery(
				io::Reader &packet,
				NetUInt64 &out_objectId
				);

			bool playerLogin(
				io::Reader &packet,
				DatabaseId &out_characterId
				);

			bool creatureQuery(
				io::Reader &packet,
				UInt32 &out_entry,
				UInt64 &out_guid
				);

			bool contactList(
				io::Reader &packet
				);

			bool addFriend(
				io::Reader &packet,
				String &out_name,
				String &out_note
				);

			bool deleteFriend(
				io::Reader &packet,
				UInt64 &out_guid
				);

			bool setContactNotes(
				io::Reader &packet
				//TODO
				);

			bool addIgnore(
				io::Reader &packet,
				String &out_name
				);

			bool deleteIgnore(
				io::Reader &packet,
				UInt64 &out_guid
				);

			bool playerLogout(
				io::Reader &packet
				);

			bool messageChat(
				io::Reader &packet,
				ChatMsg &out_type,
				Language &out_lang,
				String &out_to,
				String &out_channel,
				String &out_message
				);

			bool logoutRequest(
				io::Reader &packet
				);

			bool logoutCancel(
				io::Reader &packet
				);

			bool moveStartForward(
				io::Reader &packet,
				MovementInfo &out_info
				);

			bool moveStartBackward(
				io::Reader &packet,
				MovementInfo &out_info
				);

			bool moveStop(
				io::Reader &packet,
				MovementInfo &out_info
				);

			bool moveHeartBeat(
				io::Reader &packet,
				MovementInfo &out_info
				);

			bool setSelection(
				io::Reader &packet,
				UInt64 &out_targetGUID
				);

			bool standStateChange(
				io::Reader &packet,
				UnitStandState &out_standState
				);

			bool castSpell(
				io::Reader &packet,
				UInt32 &out_spellID,
				UInt8 &out_castCount,
				SpellTargetMap &out_targetMap
				);

			bool cancelCast(
				io::Reader &packet,
				UInt32 &out_spellID
				);

			bool itemQuerySingle(
				io::Reader &packet,
				UInt32 &out_itemID
				);

			bool attackSwing(
				io::Reader &packet,
				UInt64 &out_targetGUID
				);

			bool attackStop(
				io::Reader &packet
				);

			bool setSheathed(
				io::Reader &packet,
				UInt32 &out_sheath
				);

			bool togglePvP(
				io::Reader &packet,
				UInt8 &out_enabled
				);

			bool autoStoreLootItem(
				io::Reader &packet,
				UInt8 &out_lootSlot
				);

			bool autoEquipItem(
				io::Reader &packet,
				UInt8 &out_srcBag,
				UInt8 &out_srcSlot
				);

			bool autoStoreBagItem(
				io::Reader &packet,
				UInt8 &out_srcBag,
				UInt8 &out_srcSlot,
				UInt8 &out_dstBag
				);

			bool swapItem(
				io::Reader &packet,
				UInt8 &out_dstBag,
				UInt8 &out_dstSlot,
				UInt8 &out_srcBag,
				UInt8 &out_srcSlot
				);

			bool swapInvItem(
				io::Reader &packet,
				UInt8 &out_srcSlot,
				UInt8 &out_dstSlot
				);

			bool splitItem(
				io::Reader &packet,
				UInt8 &out_srcBag, 
				UInt8 &out_srcSlot,
				UInt8 &out_dstBag,
				UInt8 &out_dstSlot,
				UInt8 &out_count
				);

			bool autoEquipItemSlot(
				io::Reader &packet,
				UInt64 &out_itemGUID,
				UInt8 &out_dstSlot
				);

			bool destroyItem(
				io::Reader &packet,
				UInt8 &out_bag, 
				UInt8 &out_slot, 
				UInt8 &out_count, 
				UInt8 &out_data1, 
				UInt8 &out_data2, 
				UInt8 &out_data3
				);

			bool groupInvite(
				io::Reader &packet,
				String &out_memberName
				);

			bool groupAccept(
				io::Reader &packet
				);

			bool groupDecline(
				io::Reader &packet
				);

			bool groupUninvite(
				io::Reader &packet,
				String &out_memberName
				);

			bool groupUninviteGUID(
				io::Reader &packet,
				UInt64 &out_guid
				);

			bool groupSetLeader(
				io::Reader &packet,
				UInt64 &out_leaderGUID
				);

			bool lootMethod(
				io::Reader &packet,
				UInt32 &out_method,
				UInt64 &out_masterGUID,
				UInt32 &out_treshold
				);

			bool groupDisband(
				io::Reader &packet
				);

			bool requestPartyMemberStats(
				io::Reader &packet,
				UInt64 &out_GUID
				);

			bool moveWorldPortAck(
				io::Reader &packet
				);

			bool areaTrigger(
				io::Reader &packet,
				UInt32 &out_triggerId
				);

			bool setActionButton(
				io::Reader &packet,
				UInt8 &out_button,
				UInt8 &out_misc,
				UInt8 &out_type,
				UInt16 &out_action
				);

			bool gameObjectQuery(
				io::Reader &packet,
				UInt32 &out_entry,
				UInt64 &out_guid
				);

			bool forceMoveRootAck(
				io::Reader &packet
				);

			bool forceMoveUnrootAck(
				io::Reader &packet
				);

			bool cancelAura(
				io::Reader &packet,
				UInt32 &out_spellId
				);

			bool tutorialFlag(
				io::Reader &packet,
				UInt32 &out_flag
				);

			bool tutorialClear(
				io::Reader &packet
				);

			bool tutorialReset(
				io::Reader &packet
				);

			bool emote(
				io::Reader &packet,
				UInt32 &out_emote
				);

			bool textEmote(
				io::Reader &packet,
				UInt32 &out_textEmote,
				UInt32 &out_emoteNum,
				UInt64 &out_guid
				);

			bool completeCinematic(
				io::Reader &packet
				);

			bool repopRequest(
				io::Reader &packet
				);

			bool loot(
				io::Reader &packet,
				UInt64 &out_targetGuid
				);

			bool lootMoney(
				io::Reader &packet
				//TODO
				);

			bool lootRelease(
				io::Reader &packet,
				UInt64 &out_targetGuid
				);

			bool timeSyncResponse(
				io::Reader &packet,
				UInt32 &out_counter,
				UInt32 &out_ticks
				);

			bool raidTargetUpdate(
				io::Reader &packet,
				UInt8 &out_mode,
				UInt64 &out_guidOptional
				);

			bool groupRaidConvert(
				io::Reader &packet
				);

			bool groupAssistentLeader(
				io::Reader &packet,
				UInt64 &out_guid,
				UInt8 &out_flag
				);

			bool raidReadyCheck(
				io::Reader &packet,
				bool &out_hasState,
				UInt8 &out_state
				);

			bool learnTalent(
				io::Reader &packet,
				UInt32 &out_talentId,
				UInt32 &out_rank
				);

			bool useItem(
				io::Reader &packet,
				UInt8 &out_bag,
				UInt8 &out_slot,
				UInt8 &out_spellCount,
				UInt8 &out_castCount,
				UInt64 &out_itemGuid,
				SpellTargetMap &out_targetMap
				);

			bool listInventory(
				io::Reader &packet,
				UInt64 &out_guid
				);

			bool sellItem(
				io::Reader &packet,
				UInt64 &out_vendorGuid,
				UInt64 &out_itemGuid,
				UInt8 &out_count
				);
			bool buyItem(
				io::Reader &packet,
				UInt64 &out_vendorGuid,
				UInt32 &out_item,
				UInt8 &out_count
				);
			bool buyItemInSlot(
				io::Reader &packet,
				UInt64 &out_vendorGuid,
				UInt32 &out_item,
				UInt64 &out_bagGuid,
				UInt8 &out_slot,
				UInt8 &out_count
				);
		};

		namespace server_write
		{
			void triggerCinematic(
				game::OutgoingPacket &out_packet,
				UInt32 cinematicId
				);

			void charCreate(
				game::OutgoingPacket &out_packet,
				ResponseCode code
				);

			void charEnum(
				game::OutgoingPacket &out_packet,
				const game::CharEntries &characters
				);

			void charDelete(
				game::OutgoingPacket &out_packet,
				ResponseCode code
				);

			void charLoginFailed(
				game::OutgoingPacket &out_packet,
				ResponseCode code
				);

			void loginSetTimeSpeed(
				game::OutgoingPacket &out_packet,
				GameTime time
				);

			void nameQueryResponse(
				game::OutgoingPacket &out_packet,
				UInt64 objectGuid,
				const String &name,
				const String &realmName,
				UInt32 raceId,
				UInt32 genderId,
				UInt32 classId
				);

			void itemQuerySingleResponse(
				game::OutgoingPacket &out_packet,
				const ItemEntry &item
				);

			void contactList(
				game::OutgoingPacket &out_packet,
				const game::SocialInfoMap &contacts
				);

			void friendStatus(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				game::FriendResult result,
				const game::SocialInfo &info
				);

			void updateObject(
				game::OutgoingPacket &out_packet,
				const std::vector<std::vector<char>> &blocks
				);

			void destroyObject(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				bool death
				);

			void chatPlayerNotFound(
				game::OutgoingPacket &out_packet,
				const String &name
				);

			void tutorialFlags(
				game::OutgoingPacket &out_packet,
				const std::array<UInt32, 8> &tutorialData
				);

			void initializeFactions(
				game::OutgoingPacket &out_packet
				//TODO
				);

			void setProficiency(
				game::OutgoingPacket &out_packet,
				UInt8 itemClass,
				UInt32 itemSubclassMask
				);

			void actionButtons(
				game::OutgoingPacket &out_packet,
				const ActionButtons &buttons
				);

			void initialSpells(
				game::OutgoingPacket &out_packet,
				const std::vector<const SpellEntry*> &spells
				);

			void bindPointUpdate(
				game::OutgoingPacket &out_packet,
				UInt32 mapId,
				UInt32 areaId,
				float x,
				float y,
				float z
				);

			void pong(
				game::OutgoingPacket &out_packet,
				UInt32 ping
				);

			void authChallenge(
				game::OutgoingPacket &out_packet,
				UInt32 seed
				);

			void authResponse(
				game::OutgoingPacket &out_packet,
				ResponseCode code,
				Expansions expansion
				);

			void compressedUpdateObject(
				game::OutgoingPacket &out_packet,
				const std::vector<std::vector<char>> &blocks
				);

			void accountDataTimes(
				game::OutgoingPacket &out_packet,
				const std::array<UInt32, 32> &times
				);

			void setRestStart(
				game::OutgoingPacket &out_packet
				);

			void loginVerifyWorld(
				game::OutgoingPacket &out_packet,
				UInt32 mapId,
				float x,
				float y,
				float z,
				float o
				);

			void initWorldStates(
				game::OutgoingPacket &out_packet,
				UInt32 mapId,
				UInt32 zoneId
				//TODO
				);

			void addonInfo(
				game::OutgoingPacket &out_packet,
				const AddonEntries &addons
				);

			void unlearnSpells(
				game::OutgoingPacket &out_packet
				//TODO
				);

			void motd(
				game::OutgoingPacket &out_packet,
				const std::string &motd
				);

			void featureSystemStatus(
				game::OutgoingPacket &out_packet
				//TODO
				); 

			void creatureQueryResponse(
				game::OutgoingPacket &out_packet,
				const UnitEntry &unit
				);

			void setDungeonDifficulty(
				game::OutgoingPacket &out_packet
				//TODO
				);

			void timeSyncReq(
				game::OutgoingPacket &out_packet,
				UInt32 counter
				);

			void monsterMove(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				const Vector<float, 3> &oldPosition,
				const Vector<float, 3> &position,
				UInt32 time
				);

			void logoutResponse(
				game::OutgoingPacket &out_packet,
				bool success
				);

			void logoutCancelAck(
				game::OutgoingPacket &out_packet
				);

			void logoutComplete(
				game::OutgoingPacket &out_packet
				);

			void messageChat(
				game::OutgoingPacket &out_packet,
				ChatMsg type,
				Language language,
				const String &channelname,
				UInt64 targetGUID,
				const String &message,
				GameUnit *speaker
				);

			void movePacket(
				game::OutgoingPacket &out_packet,
				UInt16 opCode,
				UInt64 guid,
				const MovementInfo &movement
				);

			void moveTeleportAck(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				const MovementInfo &movement
				);

			void standStateUpdate(
				game::OutgoingPacket &out_packet,
				UnitStandState standState
				);

			void castFailed(
				game::OutgoingPacket &out_packet,
				game::SpellCastResult result,
				const SpellEntry &spell,
				UInt8 castCount
				);

			void spellStart(
				game::OutgoingPacket &out_packet,
				UInt64 casterGUID,
				UInt64 casterItemGUID,
				const SpellEntry &spell,
				const SpellTargetMap &targets,
				game::SpellCastFlags castFlags,
				Int32 castTime,
				UInt8 castCount
				);

			void spellGo(
				game::OutgoingPacket &out_packet,
				UInt64 casterGUID,
				UInt64 casterItemGUID,
				const SpellEntry &spell,
				const SpellTargetMap &targets,
				game::SpellCastFlags castFlags
				//TODO: HitInformation
				//TODO: AmmoInformation
				);

			void spellFailure(
				game::OutgoingPacket &out_packet,
				UInt64 casterGUID,
				UInt32 spellId,
				game::SpellCastResult result
				);

			void spellFailedOther(
				game::OutgoingPacket &out_packet,
				UInt64 casterGUID,
				UInt32 spellId
				);

			void spellCooldown(
				game::OutgoingPacket &out_packet
				//TODO
				);

			void cooldownEvent(
				game::OutgoingPacket &out_packet
				//TODO
				);

			void spellNonMeleeDamageLog(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt64 casterGuid,
				UInt32 spellID,
				UInt32 damage,
				UInt8 damageSchoolMask,
				UInt32 absorbedDamage,
				UInt32 resistedDamage,
				bool PhysicalDamage,
				UInt32 blockedDamage,
				bool criticalHit
				);

			void spellEnergizeLog(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt64 casterGuid,
				UInt32 spellID,
				UInt8 powerType,
				UInt32 amount
				);

			void attackStart(
				game::OutgoingPacket &out_packet,
				UInt64 attackerGUID,
				UInt64 attackedGUID
				);

			void attackStop(
				game::OutgoingPacket &out_packet,
				UInt64 attackerGUID,
				UInt64 attackedGUID
				);

			void attackStateUpdate(
				game::OutgoingPacket &out_packet,
				UInt64 attackerGUID,
				UInt64 attackedGUID,
				HitInfo hitInfo,
				UInt32 totalDamage,
				UInt32 absorbedDamage,
				UInt32 resistedDamage,
				UInt32 blockedDamage,
				VictimState targetState,
				WeaponAttack swingType,
				UInt32 damageSchool
				);

			void attackSwingBadFacing(
				game::OutgoingPacket &out_packet
				);

			void attackSwingCantAttack(
				game::OutgoingPacket &out_packet
				);

			void attackSwingDeadTarget(
				game::OutgoingPacket &out_packet
				);

			void attackSwingNotStanding(
				game::OutgoingPacket &out_packet
				);

			void attackSwingNotInRange(
				game::OutgoingPacket &out_packet
				);

			void inventoryChangeFailure(
				game::OutgoingPacket &out_packet,
				InventoryChangeFailure failure,
				GameItem *itemA,
				GameItem *itemB
				);

			void updateComboPoints(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt8 comboPoints
				);

			void spellHealLog(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt64 casterGuid,
				UInt32 spellId,
				UInt32 amount,
				bool critical
				);

			void spellDamageShield(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt64 casterGuid,
				UInt32 spellId,
				UInt32 damage,
				UInt32 dmgSchool
				);

			void periodicAuraLog(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt64 casterGuid,
				UInt32 spellId,
				UInt32 auraType,
				UInt32 damage,
				UInt32 dmgSchool,
				UInt32 absorbed,
				UInt32 resisted
				);

			void periodicAuraLog(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt64 casterGuid,
				UInt32 spellId,
				UInt32 auraType,
				UInt32 heal
				);

			void periodicAuraLog(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt64 casterGuid,
				UInt32 spellId,
				UInt32 auraType,
				UInt32 powerType,
				UInt32 amount
				);

			void updateAuraDuration(
				game::OutgoingPacket &out_packet,
				UInt8 slot,
				UInt32 durationMS
				);

			void setExtraAuraInfo(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt8 slot,
				UInt32 spellId,
				UInt32 maxDurationMS,
				UInt32 durationMS
				);

			void setExtraAuraInfoNeedUpdate(
				game::OutgoingPacket &out_packet,
				UInt64 targetGuid,
				UInt8 slot,
				UInt32 spellId,
				UInt32 maxDurationMS,
				UInt32 durationMS
				);

			void logXPGain(
				game::OutgoingPacket &out_packet,
				UInt64 victimGUID,
				UInt32 givenXP,
				UInt32 restXP,
				bool hasReferAFriendBonus
				);

			void levelUpInfo(
				game::OutgoingPacket &out_packet,
				UInt32 level,
				Int32 healthGained,
				Int32 manaGained,
				Int32 strengthGained,
				Int32 agilityGained,
				Int32 staminaGained,
				Int32 intellectGained,
				Int32 spiritGained
				);

			void groupInvite(
				game::OutgoingPacket &out_packet,
				const String &inviterName
				);

			void groupDecline(
				game::OutgoingPacket &out_packet,
				const String &inviterName
				);

			void groupUninvite(
				game::OutgoingPacket &out_packet
				);

			void groupSetLeader(
				game::OutgoingPacket &out_packet,
				const String &slotName
				);

			void groupDestroyed(
				game::OutgoingPacket &out_packet
				);

			void groupList(
				game::OutgoingPacket &out_packet,
				UInt64 receiver,
				UInt8 groupType,
				bool isBattlegroundGroup,
				UInt8 groupId,
				UInt8 assistant,
				UInt64 data1,
				const std::map<UInt64, GroupMemberSlot> &groupMembers,
				UInt64 leaderGuid,
				UInt8 lootMethod,
				UInt64 lootMasterGUID,
				UInt8 lootTreshold,
				UInt8 difficulty
				);

			void groupListRemoved(
				game::OutgoingPacket &out_packet
				);

			void partyMemberStats(
				game::OutgoingPacket &out_packet,
				const GameCharacter &character
				);

			void partyMemberStatsFull(
				game::OutgoingPacket &out_packet,
				const GameCharacter &character
				);

			void partyMemberStatsFullOffline(
				game::OutgoingPacket &out_packet,
				UInt64 offlineGUID
				);

			void partyCommandResult(
				game::OutgoingPacket &out_packet,
				PartyOperation operation,
				const String &member,
				PartyResult result
				);

			void newWorld(
				game::OutgoingPacket &out_packet,
				UInt32 newMap,
				float x,
				float y,
				float z,
				float o
				);

			void transferPending(
				game::OutgoingPacket &out_packet,
				UInt32 newMap,
				UInt32 transportId,
				UInt32 oldMap
				);

			void transferAborted(
				game::OutgoingPacket &out_packet,
				UInt32 map,
				TransferAbortReason reason
				);

			void chatWrongFaction(
				game::OutgoingPacket &out_packet
				);

			void gameObjectQueryResponse(
				game::OutgoingPacket &out_packet,
				const ObjectEntry &entry
				);

			void gameObjectQueryResponseEmpty(
				game::OutgoingPacket &out_packet,
				const UInt32 entry
				);

			void forceMoveRoot(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 unknown
				);

			void forceMoveUnroot(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 unknown
				);

			void emote(
				game::OutgoingPacket &out_packet,
				UInt32 animId,
				UInt64 guid
				);

			void textEmote(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 textEmote,
				UInt32 emoteNum,
				const String &name
				);

			void environmentalDamageLog(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt8 type,
				UInt32 damage,
				UInt32 absorb,
				UInt32 resist
				);

			void durabilityDamageDeath(
				game::OutgoingPacket &out_packet
				);

			void playSound(
				game::OutgoingPacket &out_packet,
				UInt32 soundId
				);

			void explorationExperience(
				game::OutgoingPacket &out_packet,
				UInt32 areaId,
				UInt32 experience
				);

			void aiReaction(
				game::OutgoingPacket &out_packet,
				UInt64 creatureGUID,
				UInt32 reaction
				);

			void lootResponseError(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				loot_type::Type type,
				loot_error::Type error
				);

			void lootResponse(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				loot_type::Type type,
				const LootInstance &loot
				);

			void lootReleaseResponse(
				game::OutgoingPacket &out_packet,
				UInt64 guid
				);

			void lootRemoved(
				game::OutgoingPacket &out_packet,
				UInt8 slot
				);

			void lootMoneyNotify(
				game::OutgoingPacket &out_packet,
				UInt32 moneyPerPlayer
				);

			void lootItemNotify(
				game::OutgoingPacket &out_packet
				// TODO
				);

			void lootClearMoney(
				game::OutgoingPacket &out_packet
				);

			void raidTargetUpdateList(
				game::OutgoingPacket &out_packet,
				const std::array<UInt64, 8> &list
				);

			void raidTargetUpdate(
				game::OutgoingPacket &out_packet,
				UInt8 slot,
				UInt64 guid
				);

			void raidReadyCheck(
				game::OutgoingPacket &out_packet,
				UInt64 guid
				);

			void raidReadyCheckConfirm(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt8 state
				);

			void raidReadyCheckFinished(
				game::OutgoingPacket &out_packet
				);

			void learnedSpell(
				game::OutgoingPacket &out_packet,
				UInt32 spellId
				);

			void itemPushResult(
				game::OutgoingPacket &out_packet,
				UInt64 playerGuid,
				const GameItem &item,
				bool wasLooted,
				bool wasCreated,
				UInt8 bagSlot,
				UInt8 slot,
				UInt32 addedCount,
				UInt32 totalCount
				);

			void listInventory(
				game::OutgoingPacket &out_packet,
				UInt64 vendorGuid,
				const std::vector<VendorItemEntry> &itemList
				);
		};
	}
}
