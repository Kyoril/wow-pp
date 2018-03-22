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

namespace wowpp
{
	namespace game
	{				
		/// Enumerates all OP codes sent by the client.
		namespace client_packet
		{
			enum
			{
				CharCreate						= 0x036,
				CharEnum						= 0x037,
				CharDelete						= 0x038,
				PlayerLogin						= 0x03D,
				PlayerLogout					= 0x04A,
				LogoutRequest					= 0x04B,
				LogoutCancel					= 0x04E,
				NameQuery						= 0x050,
				PetNameQuery					= 0x052,
				ItemQuerySingle					= 0x056,
				ItemQueryMultiple				= 0x057,
				QuestQuery						= 0x05C,
				GameObjectQuery					= 0x05E,
				CreatureQuery					= 0x060,
				Who								= 0x062,
				ContactList						= 0x066,
				AddFriend						= 0x069,
				DeleteFriend					= 0x06A,
				SetContactNotes					= 0x06B,
				AddIgnore						= 0x06C,
				DeleteIgnore					= 0x06D,
				GroupInvite						= 0x06E,
				GroupAccept						= 0x072,
				GroupDecline					= 0x073,
				GroupUninvite					= 0x075,
				GroupUninviteGUID				= 0x076,
				GroupSetLeader					= 0x078,
				LootMethod						= 0x07A,
				GroupDisband					= 0x07B,
				MessageChat						= 0x095,
				UseItem							= 0x0AB,
				OpenItem						= 0x0AC,
				GameObjectUse					= 0x0B1,
				AreaTrigger						= 0x0B4,
				MoveStartForward				= 0x0B5,
				MoveStartBackward				= 0x0B6,
				MoveStop						= 0x0B7,
				StartStrafeLeft					= 0x0B8,
				StartStrafeRight				= 0x0B9,
				StopStrafe						= 0x0BA,
				Jump							= 0x0BB,
				StartTurnLeft					= 0x0BC,
				StartTurnRight					= 0x0BD,
				StopTurn						= 0x0BE,
				StartPitchUp					= 0x0BF,
				StartPitchDown					= 0x0C0,
				StopPitch						= 0x0C1,
				MoveSetRunMode					= 0x0C2,
				MoveSetWalkMode					= 0x0C3,
				MoveTeleportAck					= 0x0C7,	// ACK
				MoveFallLand					= 0x0C9,
				MoveStartSwim					= 0x0CA,
				MoveStopSwim					= 0x0CB,
				SetFacing						= 0x0DA,
				SetPitch						= 0x0DB,
				MoveWorldPortAck				= 0x0DC,	// ACK
				MoveSetRawPositionAck			= 0x0E0,	// ACK
				ForceRunSpeedChangeAck			= 0x0E3,	// ACK
				ForceRunBackSpeedChangeAck		= 0x0E5,	// ACK
				ForceSwimSpeedChangeAck			= 0x0E7,	// ACK
				MoveHeartBeat					= 0x0EE,
				ForceMoveRootAck				= 0x0E9,	// ACK
				ForceMoveUnrootAck				= 0x0EB,	// ACK
				MoveKnockBackAck				= 0x0F0,	// ACK
				MoveHoverAck					= 0x0F6,	// ACK
				CompleteCinematic				= 0x0FC,
				TutorialFlag					= 0x0FE,
				TutorialClear					= 0x0FF,
				TutorialReset					= 0x100,
				StandStateChange				= 0x101,
				Emote							= 0x102,
				TextEmote						= 0x104,
				AutoStoreLootItem				= 0x108,
				AutoEquipItem					= 0x10A,
				AutoStoreBagItem				= 0x10B,
				SwapItem						= 0x10C,
				SwapInvItem						= 0x10D,
				SplitItem						= 0x10E,
				AutoEquipItemSlot				= 0x10F,
				DestroyItem						= 0x111,
				InitateTrade					= 0x116,//Trade
				BeginTrade						= 0x117,//Trade
				BusyTrade						= 0x118,//Trade later
				IgnoreTrade						= 0x119,//Trade later
				AcceptTrade						= 0x11A,//Trade
				UnacceptTrade					= 0x11B,//Trade
				CancelTrade						= 0x11C,//Trade
				SetTradeItem					= 0x11D,//Trade
				ClearTradeItem					= 0x11E,//Trade
				SetTradeGold					= 0x11F,//Trade
				SetActionButton					= 0x128,
				CastSpell						= 0x12E,
				CancelCast						= 0x12F,
				CancelAura						= 0x136,
				CancelChanneling				= 0x13B,
				SetSelection					= 0x13D,
				AttackSwing						= 0x141,
				AttackStop						= 0x142,
				RepopRequest					= 0x15A,
				ResurrectResponse				= 0x15C,
				Loot							= 0x15D,
				LootMoney						= 0x15E,
				LootRelease						= 0x15F,
				GossipHello						= 0x17B,
				QuestgiverStatusQuery			= 0x182,
				QuestgiverHello					= 0x184,
				QuestgiverQueryQuest			= 0x186,
				QuestgiverQuestAutolaunch		= 0x187,
				QuestgiverAcceptQuest			= 0x189,
				QuestgiverCompleteQuest			= 0x18A,
				QuestgiverRequestReward			= 0x18C,
				QuestgiverChooseReward			= 0x18E,
				QuestgiverCancel				= 0x190,
				QuestlogRemoveQuest				= 0x194,
				ListInventory					= 0x19E,
				SellItem						= 0x1A0,
				BuyItem							= 0x1A2,
				BuyItemInSlot					= 0x1A3,
				TrainerBuySpell					= 0x1B2,
				PlayedTime						= 0x1CC,
				MinimapPing						= 0x1D5,
				Ping							= 0x1DC,
				SetSheathed						= 0x1E0,
				AuthSession						= 0x1ED,
				ZoneUpdate						= 0x1F4,
				MailSend						= 0x238,
				MailGetList						= 0x23A,
				MailGetBody						= 0x243,
				MailTakeMoney					= 0x245,
				MailTakeItem					= 0x246,
				MailMarkAsRead					= 0x247,
				MailReturnToSender				= 0x248,
				MailDelete						= 0x249,
				MailCreateItemText				= 0x24A,
				LearnTalent						= 0x251,
				TogglePvP						= 0x253,
				RequestPartyMemberStats			= 0x27F,
				MailQueryNextTime				= 0x284,
				GroupRaidConvert				= 0x28E,
				BuyBackItem						= 0x290,
				RepairItem						= 0x2A8,
				GroupAssistentLeader			= 0x28F,
				ToggleHelm						= 0x2B9,
				ToggleCloak						= 0x2BA,
				SetActionBarToggles				= 0x2BF,
				ItemNameQuery					= 0x2C4,
				MoveFallReset					= 0x2CA,
				CharRename						= 0x2C7,
				MoveTimeSkipped					= 0x2CE,
				MoveFeatherFallAck				= 0x2CF,		// ACK
				MoveWaterWalkAck				= 0x2D0,		// ACK
				ForceWalkSpeedChangeAck			= 0x2DB,		// ACK
				ForceSwimBackSpeedChangeAck		= 0x2DD,		// ACK
				ForceTurnRateChangeAck			= 0x2DF,		// ACK
				RaidTargetUpdate				= 0x321,
				RaidReadyCheck					= 0x322,
				SetDungeonDifficulty			= 0x329,
				MoveFlightAck					= 0x340,		// ACK
				MoveSetCanFlyAck				= 0x345,		// ACK
				MoveSetFly						= 0x346,
				MoveStartAscend					= 0x359,
				MoveStopAscend					= 0x35A,
				ForceFlightSpeedChangeAck		= 0x382,		// ACK
				ForceFlightBackSpeedChangeAck	= 0x384,		// ACK
				RealmSplit						= 0x38C,
				MoveChangeTransport				= 0x38D,
				TimeSyncResponse				= 0x391,
				MoveStartDescend				= 0x3A7,
				VoiceSessionEnable				= 0x3AF,
				RaidReadyCheckFinished			= 0x3C5,
				GetChannelMemberCount			= 0x3D3,
				QuestgiverStatusMultipleQuery	= 0x416
			};
		}

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
				PetNameQueryResponse		= 0x053,
				ItemQuerySingleResponse		= 0x058,
				ItemQueryMultipleResponse	= 0x059,
				QuestQueryResponse			= 0x05D,
				GameObjectQueryResponse		= 0x05F,
				CreatureQueryResponse		= 0x061,
				WhoResponse					= 0x063,
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
				SetRunSpeed					= 0x0CD,
				SetRunBackSpeed				= 0x0CF,
				SetWalkSpeed				= 0x0D1,
				SetSwimSpeed				= 0x0D3,
				SetSwimBackSpeed			= 0x0D5,
				MonsterMove					= 0x0DD,
				MoveWaterWalk				= 0x0DE,
				MoveLandWalk				= 0x0DF,
				RunSpeedChange				= 0x0E2,
				RunBackSpeedChange			= 0x0E4,
				SwimSpeedChange				= 0x0E6,
				ForceMoveRoot				= 0x0E8,
				ForceMoveUnroot				= 0x0EA,
				MoveRoot					= 0x0EC,
				MoveUnroot					= 0x0ED,
				MoveHeartBeat				= 0x0EE,
				MoveKnockBack				= 0x0EF,	// Sent ONLY to affected client
				MoveKnockBack2				= 0x0F1,	// Sent to other clients that are not affected
				MoveFeatherFall				= 0x0F2,
				MoveNormalFall				= 0x0F3,
				MoveSetHover				= 0x0F4,
				MoveUnsetHover				= 0x0F5,
				TutorialFlags				= 0x0FD,
				Emote						= 0x103,
				TextEmote					= 0x105,
				InventoryChangeFailure		= 0x112,
				TradeStatus					= 0x120,   //Trade
				TradeStatusExtended			= 0x121,   //Trade
				InitializeFactions			= 0x122,
				SetProficiency				= 0x127,
				ActionButtons				= 0x129,
				InitialSpells				= 0x12A,
				LearnedSpell				= 0x12B,
				SupercededSpell				= 0x12C,
				CastFailed					= 0x130,
				SpellStart					= 0x131,
				SpellGo						= 0x132,
				SpellFailure				= 0x133,
				SpellCooldown				= 0x134,
				CooldownEvent				= 0x135,
				UpdateAuraDuration			= 0x137,
				ChannelStart				= 0x139,
				ChannelUpdate				= 0x13A,
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
				ResurrectRequest			= 0x15B,
				LootResponse				= 0x160,
				LootReleaseResponse			= 0x161,
				LootRemoved					= 0x162,
				LootMoneyNotify				= 0x163,
				LootItemNotify				= 0x164,
				LootClearMoney				= 0x165,
				ItemPushResult				= 0x166,
				PetSpells					= 0x179,
				GossipMessage				= 0x17D,
				GossipComplete				= 0x17E,
				QuestgiverStatus			= 0x183,
				QuestgiverQuestList			= 0x185,
				QuestgiverQuestDetails		= 0x188,
				QuestgiverRequestItems		= 0x18B,
				QuestgiverOfferReward		= 0x18D,
				QuestgiverQuestComplete		= 0x191,
				QuestlogFull				= 0x195,
				QuestupdateComplete			= 0x198,
				QuestupdateAddKill			= 0x199,
				QuestupdateAddItem			= 0x19A,		// Unused?
				ListInventory				= 0x19F,
				SellItem					= 0x1A1,
				TrainerList					= 0x1B1,
				TrainerBuySucceeded			= 0x1B3,
				TrainerBuyFailed			= 0x1B4,
				Notification				= 0x1CB,
				PlayedTime					= 0x1CD,
				LogXPGain					= 0x1D0,
				MinimapPing					= 0x1D5,
				Pong						= 0x1DD,
				ClearCooldown				= 0x1DE,
				LevelUpInfo					= 0x1D4,
				SpellDelayed				= 0x1E2,
				AuthChallenge				= 0x1EC,
				AuthResponse				= 0x1EE,
				PlaySpellVisual				= 0x1F3,
				CompressedUpdateObject		= 0x1F6,
				PlaySpellImpact				= 0x1F7,
				ExplorationExperience		= 0x1F8,
				EnvironmentalDamageLog		= 0x1FC,
				AccountDataTimes			= 0x209,
				ChatWrongFaction			= 0x219,
				SetRestStart				= 0x21E,
				LoginVerifyWorld			= 0x236,
				MailSendResult				= 0x239,
				MailListResult				= 0x23B,
				MailSendBody				= 0x244,
				SpellLogMiss				= 0x24B,
				PeriodicAuraLog				= 0x24E,
				SpellDamageShield			= 0x24F,
				SpellNonMeleeDamageLog		= 0x250,
				SetFlatSpellModifier		= 0x266,
				SetPctSpellModifier			= 0x267,
				MailQueryNextTime			= 0x284,
				MailReceived				= 0x285,
				StandStateUpdate			= 0x29D,
				LootStartRoll				= 0x2A1,
				SpellFailedOther			= 0x2A6,
				ChatPlayerNotFound			= 0x2A9,
				DurabilityDamageDeath		= 0x2BD,
				InitWorldStates				= 0x2C2,
				ItemNameQueryResponse		= 0x2C5,
				CharRename					= 0x2C8,
				PlaySound					= 0x2D2,
				WalkSpeedChange				= 0x2DA,
				SwimBackSpeedChange			= 0x2DC,
				TurnRateChange				= 0x2DE,
				AddonInfo					= 0x2EF,
				PartyMemberStatsFull		= 0x2F2,
				MoveTimeSkipped				= 0x319,
				RaidTargetUpdate			= 0x321,
				RaidReadyCheck				= 0x322,
				SetDungeonDifficulty		= 0x329,
				Motd						= 0x33D,
				MoveSetCanFly				= 0x343,
				MoveUnsetCanFly				= 0x344,
				SetFlightSpeed				= 0x37E,
				SetFlightBackSpeed			= 0x380,
				FlightSpeedChange			= 0x381,
				FlightBackSpeedChange		= 0x383,
				TimeSyncReq					= 0x390,
				UpdateComboPoints			= 0x39D,
				SetExtraAuraInfo			= 0x3A4,
				SetExtraAuraInfoNeedUpdate	= 0x3A5,
				ClearExtraAuraInfo			= 0x3A6,
				RaidReadyCheckConfirm		= 0x3AE,
				RaidReadyCheckFinished		= 0x3C5,
				FeatureSystemStatus			= 0x3C8,
				QuestgiverStatusMultiple	= 0x417,
				UnlearnSpells				= 0x41D
			};
		}
	}
}