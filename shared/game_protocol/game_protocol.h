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
#include "game/action_button.h"
#include "game/game_unit.h"
#include "game/movement_info.h"
#include "game/spell_target_map.h"
#include "proto_data/project.h"
#include "op_codes.h"
#include "game/mail.h"

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

		namespace transfer_abort_reason
		{
			enum Type
			{
				None = 0x0000,
				/// Transfer Aborted: instance is full
				MaxPlayers = 0x0001,
				/// Transfer aborted: instance not found
				NotFound = 0x0002,
				/// You have entered too many instances recently.
				TooManyInstances = 0x0003,
				/// Unable to zone in while an encounter is in progress.
				ZoneInCombat = 0x0005,
				/// You must have TBC expansion installed to access this area.
				InsufExpanLevel1 = 0x0106,
				/// Normal difficulty mode is not available for %s.
				Difficulty1 = 0x0007,
				/// Heroic difficulty mode is not available for %s.
				Difficulty2 = 0x0107,
				/// Epic difficulty mode is not available for %s.
				Difficulty3 = 0x0207
			};
		}

		typedef transfer_abort_reason::Type TransferAbortReason;

		namespace session_status
		{
			enum Type
			{
				/// Never process this packet.
				Never = 0x00,
				/// Always process this packet.
				Always = 0x01,
				/// Process this packet if the player is connected but not yet authenticated.
				Connected = 0x02,
				/// Process this packet if the player is authenticated.
				Authentificated = 0x03,
				/// Process this packet if the player entered a world.
				LoggedIn = 0x04,
				/// Process this packet only if there is a pending transfer
				TransferPending = 0x05,
				/// Process this packet only if the player is authenticated but hasn't entered a world.
				NotLoggedIn = 0x06
			};
		}

		typedef session_status::Type SessionStatus;

		namespace sell_error
		{
			enum Type
			{
				/// Item not found.
				CantFindItem = 1,
				/// Merchant doesn't like that item.
				CantSellItem = 2,
				/// Merchant doesn't like you.
				CantFindVendor = 3,
				/// You don't own that item.
				YouDontOwnThatItem = 4,
				/// Can only do with empty bags.
				OnlyEmptyBag = 5
			};
		}

		typedef sell_error::Type SellError;

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

		struct AddonEntry
		{
			String addonNames;
			UInt8 unk6;
			UInt32 crc, unk7;
		};

		typedef std::vector<AddonEntry> AddonEntries;

		struct QuestMenuItem
		{
			const proto::QuestEntry *quest;
			UInt8 menuIcon;
			Int32 questLevel;
			String title;
		};
		
		namespace atlogin_flags
		{
			enum Type
			{
				/// Nothing special happens at login.
				None = 0x00,
				/// Player will be forced to rename his character before entering the world.
				Rename = 0x01,
				/// Character spellbook will be reset (unlearn all learned spells).
				ResetSpells = 0x02,
				/// Character talents will be reset.
				ResetTalents = 0x04,
				/// Indicates that this character never logged in before.
				FirstLogin = 0x20,
			};
		}

		typedef atlogin_flags::Type AtLoginFlags;

		namespace character_flags
		{
			enum Type
			{
				None = 0x00000000,
				LockedForTransfer = 0x00000004,
				HideHelm = 0x00000400,
				HideCloak = 0x00000800,
				Ghost = 0x00002000,
				Rename = 0x00004000,
				LockedByBilling = 0x01000000,
				Declined = 0x02000000
			};
		}

		typedef character_flags::Type CharacterFlags;

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
			CharacterFlags flags;
			math::Vector3 location;
			float o;
			bool cinematic;
			std::map<UInt8, const proto::ItemEntry *> equipment;
			AtLoginFlags atLogin;

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
				, flags(character_flags::None)
				, location(0.0f, 0.0f, 0.0f)
				, o(0.0f)
				, cinematic(true)
				, atLogin(atlogin_flags::None)
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
				CharLoginLockedByBilling		= 0x08,

				CharNameSuccess					= 0x4A,
				CharNameFailure					= 0x4B,
				CharNameNoName					= 0x4C,
				CharNameTooShort				= 0x4D,
				CharNameTooLong					= 0x4E,
				CharNameInvalidCharacters		= 0x4F,
				CharNameMixedLanguages			= 0x50,
				CharNameProfane					= 0x51,
				CharNameReserved				= 0x52,
				CharNameInvalidApostrophe		= 0x53,
				CharNameMultipleApostrophes		= 0x54,
				CharNameThreeConsecutive		= 0x55,
				CharNameInvalidSpace			= 0x56,
				CharNameConsecutiveSpaces		= 0x57,
				CharNameRussianConsecutiveSilentCharacters	= 0x58,
				CharNameRussianSilentCharacterAtBeginningOrEnd	= 0x59,
				CharNameDeclensionDoesntMatchBaseName	= 0x5A,
			};
		}

		typedef response_code::Type ResponseCode;

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

		struct WhoListRequest final
		{
			UInt32 level_min;
			UInt32 level_max;
			UInt32 racemask;
			UInt32 classmask;
			std::vector<UInt32> zoneids;
			std::vector<String> strings;
			String player_name;
			String guild_name;

			WhoListRequest()
				: level_min(0)
				, level_max(100)
				, racemask(0)
				, classmask(0)
			{
			}
		};

		struct WhoResponseEntry final
		{
			UInt32 level;
			String name;
			String guild;
			UInt32 race;
			UInt32 class_;
			UInt32 zone;
			UInt8 gender;

			WhoResponseEntry()
				: level(1)
				, race(0)
				, class_(0)
				, zone(0)
				, gender(0)
			{
			}
			WhoResponseEntry(const GameCharacter &character);
		};

		struct WhoResponse final
		{
			std::vector<WhoResponseEntry> entries;

			WhoResponse()
			{
			}
		};
		
		io::Reader &operator >>(io::Reader &r, WhoListRequest &out_whoList);

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

			bool minimapPing(
				io::Reader &packet,
				float &out_x,
				float &out_y
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

			bool zoneUpdate(
				io::Reader &packet,
				UInt32 &out_zoneId
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

			bool gossipHello(
			    io::Reader &packet,
			    UInt64 &out_npcGuid
			);

			bool trainerBuySpell(
			    io::Reader &packet,
			    UInt64 &out_guid,
			    UInt32 &out_spell
			);

			bool realmSplit(
			    io::Reader &packet,
			    UInt32 &out_preferredRealm
			);

			bool voiceSessionEnable(
			    io::Reader &packet,
			    UInt16 &out_unknown
			);

			bool charRename(
			    io::Reader &packet,
			    UInt64 &out_guid,
			    String &out_name
			);

			bool questgiverStatusQuery(
			    io::Reader &packet,
			    UInt64 &out_guid
			);

			bool questgiverHello(
			    io::Reader &packet,
			    UInt64 &out_guid
			);

			bool questgiverQueryQuest(
			    io::Reader &packet,
			    UInt64 &out_guid,
			    UInt32 &out_questId
			);

			bool questgiverQuestAutolaunch(
			    io::Reader &packet
			    // Empty?
			);

			bool questgiverAcceptQuest(
			    io::Reader &packet,
			    UInt64 &out_guid,
			    UInt32 &out_questId
			);

			bool questgiverCompleteQuest(
			    io::Reader &packet,
			    UInt64 &out_guid,
			    UInt32 &out_questId
			);

			bool questgiverRequestReward(
			    io::Reader &packet,
			    UInt64 &out_guid,
			    UInt32 &out_questId
			);

			bool questgiverChooseReward(
			    io::Reader &packet,
			    UInt64 &out_guid,
			    UInt32 &out_questId,
			    UInt32 &out_reward
			);

			bool questgiverCancel(
			    io::Reader &packet
			    // Empty
			);

			bool questQuery(
			    io::Reader &packet,
			    UInt32 &out_questId
			);

			bool questgiverStatusMultiple(
			    io::Reader &packet
			);

			bool questlogRemoveQuest(
			    io::Reader &packet,
			    UInt8 &out_questIndex
			);

			bool gameobjectUse(
				io::Reader &packet,
				UInt64 &out_guid
				);

			bool openItem(
				io::Reader &packet,
				UInt8 &out_bag,
				UInt8 &out_slot
				);

			bool moveTimeSkipped(
				io::Reader &packet,
				UInt64 &out_guid,
				UInt32 &out_timeSkipped
				);

			bool who(
				io::Reader &packet, 
				WhoListRequest &out_whoList
				);

			bool initateTrade(
				io::Reader &packet,
				UInt64 &guid
				);

			bool beginTrade(
				io::Reader &packet
				);
			bool acceptTrade(
				io::Reader &packet
				);
			bool setTradeGold(
				io::Reader &packet,
				UInt32 &gold
				);
			bool setTradeItem(
				io::Reader &packet,
				UInt8 &tradeSlot,
				UInt8 &bag,
				UInt8 &slot
				);
			bool clearTradeItem(
				io::Reader &packet,
				UInt8 &slot
			);
			bool petNameRequest(
				io::Reader &packet,
				UInt32 &out_petNumber,
				UInt64 &out_petGUID
				);

			bool setActionBarToggles(
				io::Reader &packet,
				UInt8 &out_actionBars
				);
			
			bool mailSend(
				io::Reader &packet,
				ObjectGuid &out_mailboxGuid,
				MailData &out_mail
				);

			bool mailGetList(
				io::Reader &packet,
				ObjectGuid &out_mailboxGuid
				);

			bool mailGetBody(
				io::Reader &packet,
				UInt32 &out_mailTextId,
				UInt32 &out_mailId
				);

			bool mailMarkAsRead(
				io::Reader &packet,
				ObjectGuid &out_mailboxGuid,
				UInt32 &out_mailId
				);

			bool mailQueryNextTime(
				io::Reader &packet
				);

			bool mailTakeMoney(
				io::Reader &packet,
				ObjectGuid &out_mailboxGuid,
				UInt32 &out_mailId
				);

			bool mailDelete(
				io::Reader &packet,
				ObjectGuid &out_mailboxGuid,
				UInt32 &out_mailId
			);

			bool resurrectResponse(
				io::Reader &packet,
				UInt64 &out_guid,
				UInt8 &out_status
				);

			bool cancelChanneling(
				io::Reader &packet
				);

			bool itemNameQuery(
				io::Reader &packet,
				UInt32 &out_entry,
				UInt64 &out_guid
			);

			bool repairItem(
				io::Reader &packet,
				UInt64 &out_npcGuid,
				UInt64 &out_itemGuid,
				UInt8 &out_guildBank
			);

			bool buyBackItem(
				io::Reader &packet,
				UInt64 &out_vendorGuid,
				UInt32 &out_slot
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
				Int32 localeIndex,
			    const proto::ItemEntry &item
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

			void minimapPing(
				game::OutgoingPacket &out_packet,
				UInt64 senderGuid,
				float x,
				float y
			);

			void tutorialFlags(
			    game::OutgoingPacket &out_packet,
			    const std::array<UInt32, 8> &tutorialData
			);

			void initializeFactions(
			    game::OutgoingPacket &out_packet,
				const GameCharacter &character
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
			    const proto::Project &project,
			    const std::vector<const proto::SpellEntry *> &spells,
			    const GameUnit::CooldownMap &cooldowns
			);

			void bindPointUpdate(
			    game::OutgoingPacket &out_packet,
			    UInt32 mapId,
			    UInt32 areaId,
			    const math::Vector3 &location
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
			    math::Vector3 location,
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
				Int32 localeIndex,
			    const proto::UnitEntry &unit
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
			    const math::Vector3 &oldPosition,
				const std::vector<math::Vector3> &path,
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
			    const proto::SpellEntry &spell,
			    UInt8 castCount
			);

			void spellStart(
			    game::OutgoingPacket &out_packet,
			    UInt64 casterGUID,
			    UInt64 casterItemGUID,
			    const proto::SpellEntry &spell,
			    const SpellTargetMap &targets,
			    game::SpellCastFlags castFlags,
			    Int32 castTime,
			    UInt8 castCount
			);

			void spellGo(
			    game::OutgoingPacket &out_packet,
			    UInt64 casterGUID,
			    UInt64 casterItemGUID,
			    const proto::SpellEntry &spell,
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
			    game::OutgoingPacket &out_packet,
			    UInt64 targetGUID,
			    UInt8 flags,
			    const std::map<UInt32, UInt32> &spellCooldownTimesMS
			);

			void cooldownEvent(
			    game::OutgoingPacket &out_packet,
			    UInt32 spellID,
			    UInt64 objectGUID
			);

			void clearCooldown(
			    game::OutgoingPacket &out_packet,
			    UInt32 spellID,
			    UInt64 targetGUID
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

			void clearExtraAuraInfo(
				game::OutgoingPacket &out_packet,
				UInt64 casterGuid,
				UInt32 spellId
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
			    math::Vector3 location,
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
				Int32 localeIndex,
			    const proto::ObjectEntry &entry
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
				UInt64 playerGuid,
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

			void supercededSpell(
				game::OutgoingPacket &out_packet,
				UInt32 prevSpellId,
				UInt32 newSpellId
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
			    const proto::ItemManager &itemManager,
			    const std::vector<proto::VendorItemEntry> &itemList
			);

			void trainerList(
			    game::OutgoingPacket &out_packet,
			    const GameCharacter &character,
			    UInt64 trainerGuid,
			    const proto::TrainerEntry &trainerEntry
			);

			void gossipMessage(
			    game::OutgoingPacket &out_packet,
			    UInt64 objectGuid,
			    UInt32 titleTextId
			);

			void trainerBuySucceeded(
			    game::OutgoingPacket &out_packet,
			    UInt64 trainerGuid,
			    UInt32 spellId
			);

			void playSpellVisual(
			    game::OutgoingPacket &out_packet,
			    UInt64 unitGuid,
			    UInt32 visualId
			);

			void playSpellImpact(
			    game::OutgoingPacket &out_packet,
			    UInt64 unitGuid,
			    UInt32 spellId
			);

			void charRename(
			    game::OutgoingPacket &out_packet,
			    game::ResponseCode response,
			    UInt64 unitGuid,
			    const String &newName
			);

			void changeSpeed(
			    game::OutgoingPacket &out_packet,
			    MovementType moveType,
			    UInt64 guid,
			    float speed,
				UInt32 counter
			);

			void questgiverStatus(
			    game::OutgoingPacket &out_packet,
			    UInt64 guid,
			    game::QuestgiverStatus status
			);

			void questgiverQuestList(
			    game::OutgoingPacket &out_packet,
			    UInt64 guid,
			    const String &title,
			    UInt32 emoteDelay,
			    UInt32 emote,
			    const std::vector<QuestMenuItem> &menu
			);

			void questgiverQuestDetails(
			    game::OutgoingPacket &out_packet,
			    UInt64 guid,
			    const proto::ItemManager &items,
			    const proto::QuestEntry &quest
			);

			void questQueryResponse(
			    game::OutgoingPacket &out_packet,
			    const proto::QuestEntry &quest
			);

			void gossipComplete(
			    game::OutgoingPacket &out_packet
			);

			void questgiverStatusMultiple(
			    game::OutgoingPacket &out_packet,
			    const std::map<UInt64, game::QuestgiverStatus> &status
			);

			void questgiverRequestItems(
			    game::OutgoingPacket &out_packet,
			    UInt64 guid,
			    bool closeOnCancel,
			    bool enableNext,
			    const proto::ItemManager &items,
			    const proto::QuestEntry &quest
			);

			void questgiverOfferReward(
			    game::OutgoingPacket &out_packet,
			    UInt64 guid,
			    bool enableNext,
			    const proto::ItemManager &items,
			    const proto::QuestEntry &quest
			);

			void questgiverQuestComplete(
			    game::OutgoingPacket &out_packet,
			    bool isMaxLevel,
			    UInt32 xp,
			    const proto::QuestEntry &quest
			);

			void questlogFull(
			    game::OutgoingPacket &out_packet
			);

			void questupdateAddKill(
			    game::OutgoingPacket &out_packet,
			    UInt32 questId,
			    UInt32 entry,
			    UInt32 totalCount,
			    UInt32 maxCount,
			    UInt64 guid
			);

			void setFlatSpellModifier(
				game::OutgoingPacket &out_packet,
				UInt8 bit,
				UInt8 modOp,
				Int32 value
				);

			void setPctSpellModifier(
				game::OutgoingPacket &out_packet,
				UInt8 bit,
				UInt8 modOp,
				Int32 value
				);

			void spellDelayed(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 delayTimeMS
				);

			void sellItem(
				game::OutgoingPacket &out_packet,
				SellError errorCode,
				UInt64 vendorGuid,
				UInt64 itemGuid,
				UInt32 param
				);

			void questupdateComplete(
				game::OutgoingPacket &out_packet,
				UInt32 questId
				);

			void moveTimeSkipped(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 timeSkipped
				);

			void whoRequestResponse(
				game::OutgoingPacket &out_packet,
				const game::WhoResponse &response,
				UInt32 matchcount
				);

			void spellLogMiss(
				game::OutgoingPacket &out_packet,
				UInt32 spellID,
				UInt64 caster,
				UInt8 unknown,
				const std::map<UInt64, game::SpellMissInfo> &missedTargetGUIDs
				);

			void sendTradeStatus(
				game::OutgoingPacket &out_packet,
				UInt32 status,
				UInt64 guid,
				bool sendError = false,
				UInt32 errorCode = 0,
				UInt32 itemCategoryEntry = 0
				);

			void sendUpdateTrade(
				game::OutgoingPacket &out_packet, 
				UInt8 state, 
				UInt32 tradeID, 
				UInt32 next_slot, 
				UInt32 prev_slot, 
				UInt32 gold, 
				UInt32 spell,
				std::array<std::shared_ptr<GameItem>, 7> items
				);

			void petNameQueryResponse(
				game::OutgoingPacket &out_packet,
				UInt32 petNumber,
				const String &petName,
				UInt32 petNameTimestmap
				);
			
			void petSpells(
				game::OutgoingPacket &out_packet,
				UInt64 petGUID
				);

			void mailListResult(
				game::OutgoingPacket &out_packet,
				std::list<Mail> mails
				);

			void mailSendResult(
				game::OutgoingPacket &out_packet,
				MailResult mailResult
				);

			void mailQueryNextTime(
				game::OutgoingPacket &out_packet,
				UInt32 unreadMails,
				std::list<Mail> &mails
				);

			void mailReceived(
				game::OutgoingPacket &out_packet,
				UInt32 mailId
				);

			void mailSendBody(
				game::OutgoingPacket &out_packet,
				UInt32 mailTextId,
				const String &body
				);

			void moveSetCanFly(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 counter
			);

			void moveUnsetCanFly(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 counter
			);
			
			void moveFeatherFall(
				game::OutgoingPacket &out_packet,
				UInt64 guid
			);
			
			void moveNormalFall(
				game::OutgoingPacket &out_packet,
				UInt64 guid
			);

			void moveSetHover(
				game::OutgoingPacket &out_packet,
				UInt64 guid
			);

			void moveUnsetHover(
				game::OutgoingPacket &out_packet,
				UInt64 guid
			);

			void moveWaterWalk(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 counter
			);

			void moveLandWalk(
				game::OutgoingPacket &out_packet,
				UInt64 guid,
				UInt32 counter
			);

			void lootStartRoll(
				game::OutgoingPacket &out_packet,
				UInt64 itemGuid,
				UInt32 playerCount,
				UInt32 itemEntry,
				UInt32 itemSuffix,
				UInt32 itemPropId,
				UInt32 countdown
			);

			void playedTime(
				game::OutgoingPacket &out_packet,
				UInt32 totalTimeInSecs,
				UInt32 levelTimeInSecs
			);

			void resurrectRequest(
				game::OutgoingPacket &out_packet,
				UInt64 objectGUID,
				const String &sentName,
				UInt8 typeId
			);

			void channelStart(
				game::OutgoingPacket &out_packet,
				UInt64 casterGUID,
				UInt32 spellId,
				Int32 duration
			);

			void channelUpdate(
				game::OutgoingPacket &out_packet,
				UInt64 casterGUID,
				Int32 castTime
			);

			void moveKnockBack(
				game::OutgoingPacket &out_packet,
				UInt64 targetGUID,
				float vcos,
				float vsin,
				float speedxy,
				float speedz
			);

			void moveKnockBackWithInfo(
				game::OutgoingPacket &out_packet,
				UInt64 targetGUID,
				const MovementInfo &movementInfo
			);

			void itemNameQueryResponse(
				game::OutgoingPacket &out_packet,
				UInt32 entryId,
				const String &name,
				UInt32 inventoryType
			);

			void notification(
				game::OutgoingPacket &out_packet,
				const String &message
			);
		};
	}
}
