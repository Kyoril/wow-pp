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
#include <array>
#include <vector>
#include <functional>

namespace wowpp
{
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
				CreatureQuery			= 0x060,
				ContactList				= 0x066,
				AddFriend				= 0x069,
				DeleteFriend			= 0x06A,
				SetContactNotes			= 0x06B,
				AddIgnore				= 0x06C,
				DeleteIgnore			= 0x06D,
				MessageChat				= 0x095,
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
				MoveHeartBeat			= 0x0EE,
				StandStateChange		= 0x101,
				CastSpell				= 0x12E,
				CancelCast				= 0x12F,
				SetSelection			= 0x13D,
				AttackSwing				= 0x141,
				AttackStop				= 0x142,
				Ping					= 0x1DC,
				SetSheathed				= 0x1E0,
				AuthSession				= 0x1ED,
				MoveFallReset			= 0x2CA,
				SetDungeonDifficulty	= 0x329,
				MoveSetFly				= 0x346,
				MoveStartAscend			= 0x359,
				MoveStopAscend			= 0x35A,
				MoveChangeTransport		= 0x38D,
				MoveStartDescend		= 0x3A7
			};
		}

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
				CharacterLoginFailed		= 0x041,
				LoginSetTimeSpeed			= 0x042,
				LogoutResponse				= 0x04C,
				LogoutComplete				= 0x04D,
				LogoutCancelAck				= 0x04F,
				NameQueryResponse			= 0x051,
				ItemQuerySingleResponse		= 0x058,
				ItemQueryMultipleResponse	= 0x059,
				CreatureQueryResponse		= 0x061,
				ContactList					= 0x067,
				FriendStatus				= 0x068,
				MessageChat					= 0x096,
				UpdateObject				= 0x0A9,
				DestroyObject				= 0x0AA,
				MonsterMove					= 0x0DD,
				MoveRoot					= 0x0EC,
				MoveUnroot					= 0x0ED,
				MoveHeartBeat				= 0x0EE,
				TutorialFlags				= 0x0FD,
				InitializeFactions			= 0x122,
				SetProficiency				= 0x127,
				ActionButtons				= 0x129,
				InitialSpells				= 0x12A,
				CastFailed					= 0x130,
				SpellStart					= 0x131,
				SpellGo						= 0x132,
				SpellFailure				= 0x133,
				SpellCooldown				= 0x134,
				CooldownEvent				= 0x135,
				AttackStart					= 0x143,
				AttackStop					= 0x144,
				AttackSwingNotInRange		= 0x145,
				AttackSwingBadFacing		= 0x146,
				AttackSwingNotStanding		= 0x147,
				AttackSwingDeadTarget		= 0x148,
				AttackSwingCantAttack		= 0x149,
				AttackerStateUpdate			= 0x14A,
				SpellEnergizeLog			= 0x151,
				BindPointUpdate				= 0x155,
				Pong						= 0x1DD,
				AuthChallenge				= 0x1EC,
				AuthResponse				= 0x1EE,
				CompressedUpdateObject		= 0x1F6,
				AccountDataTimes			= 0x209,
				SetRestStart				= 0x21E,
				LoginVerifyWorld			= 0x236,
				SpellNonMeleeDamageLog		= 0x250,
				StandStateUpdate			= 0x29D,
				SpellFailedOther			= 0x2A6,
				ChatPlayerNotFound			= 0x2A9,
				InitWorldStates				= 0x2C2,
				AddonInfo					= 0x2EF,
				SetDungeonDifficulty		= 0x329,
				Motd						= 0x33D,
				TimeSyncReq					= 0x390,
				FeatureSystemStatus			= 0x3C8,
				UnlearnSpells				= 0x41D
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
				game::OutgoingPacket &out_packet
				//TODO
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
		};
	}
}
