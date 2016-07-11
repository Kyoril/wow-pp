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

#include "game/pch.h"
#include "common/typedefs.h"

namespace wowpp
{
	namespace mail_message_type
	{
		enum Enum
		{
			Normal = 0,
			Auction = 2,
			Creature = 3,
			GameObject = 4,
			Item = 5
		};
	}
	typedef mail_message_type::Enum MailMessageType;

	namespace mail_check_mask
	{
		enum Enum
		{
			None = 0x00,
			Read = 0x01,
			Returned = 0x02,
			Copied = 0x04,
			CODPayment = 0x08,
			HasBody = 0x10
		};
	}
	typedef mail_check_mask::Enum MailCheckMask;

	namespace mail_stationery
	{
		enum Enum
		{
			Unknown = 1,
			Default = 41,
			GM = 61,
			Auction = 62,
			Val = 64,
			Chr = 65
		};
	}
	typedef mail_stationery::Enum MailStationery;

	namespace mail_state
	{
		enum Enum
		{
			Unchanged = 1,
			Changed = 2,
			Deleted = 3
		};
	}
	typedef mail_state::Enum MailState;

	// Not used at the moment
	namespace mail_auction_answers
	{
		enum Enum
		{
			Outbidded = 0,
			Won = 1,
			Successful = 2,
			Expired = 3,
			CancelledToBidder = 4,
			Canceled = 5,
			SalePending = 6
		};
	}
	typedef mail_auction_answers::Enum MailAuctionAnswers;

	class GameItem;
	class GameObject;
	class Player;

	class MailSender
	{
	public:
	MailSender() : m_messageType(mail_message_type::Normal), m_senderId(0), m_stationery(mail_stationery::Default) {}
	
		/**
		* Creates a new MailSender object.
		*
		* @param messageType the type of the mail.
		* @param sender_guidlow_or_entry The lower part of the GUID of the player sending
		* this mail, or the Entry of the non-player object.
		* @param stationery The stationary associated with this MailSender.
		*
		*/
		MailSender(MailMessageType messageType,
					UInt64 sender_guidlow_or_entry,
					MailStationery stationery)
		: m_messageType(messageType), m_senderId(sender_guidlow_or_entry), m_stationery(stationery)
		{
		}
		MailSender(GameObject* sender, MailStationery stationery = mail_stationery::Default);
		/// The Messagetype of this MailSender.
		MailMessageType getMessageType() const { return m_messageType; }
		/// The GUID of the player represented by this MailSender, or the Entry of the non-player object.
		UInt64 getSenderId() const { return m_senderId; }
		/// The stationary associated with this MailSender
		MailStationery getStationery() const { return m_stationery; }
	private:
		// Trap for wrong used guid as low guid, no body
		MailSender(MailMessageType messageType, UInt64 wrong_guid, MailStationery stationery = mail_stationery::Default);

		MailMessageType m_messageType;
		UInt64 m_senderId;                                  // player low guid or other object entry
		MailStationery m_stationery;
	};

	class MailReceiver
	{
	public:                                                 // Constructors
		explicit MailReceiver(UInt64 receiver_guid) : m_receiver(nullptr), m_receiver_guid(receiver_guid) {}
		MailReceiver(Player* receiver);
		MailReceiver(Player* receiver, UInt64 receiver_guid);
	public:                                                 // Accessors
															/**
															* Gets the player associated with this MailReciever
															*
															* @see Player
															*
															* @returns a pointer to the Player this mail is for.
															*
															*/
		Player* GetPlayer() const { return m_receiver; }
		/**
		* Gets the low part of the recievers GUID.
		*
		* @returns the low part of the GUID of the player associated with this MailReciever
		*/
		UInt64 const& GetPlayerGuid() const { return m_receiver_guid; }
	private:
		Player* m_receiver;
		UInt64 m_receiver_guid;
	};

	class MailDraft
	{
		/**
		* Holds a Map of GUIDs of items and pointers to the items.
		*/
		typedef std::map<UInt32, GameItem*> MailItemMap;

	public:                                                 // Constructors
															/**
															* Creates a new blank MailDraft object
															*
															*/
		MailDraft()
			: m_mailTemplateId(0), m_mailTemplateItemsNeed(false), m_bodyId(0), m_money(0), m_COD(0) {}
		/**
		* Creates a new MailDraft object using mail template id.
		*
		* @param mailTemplateId The ID of the Template to be used.
		* @param a boolean specifying whether the mail needs items or not.
		*
		*/
		explicit MailDraft(UInt16 mailTemplateId, bool need_items = true)
			: m_mailTemplateId(mailTemplateId), m_mailTemplateItemsNeed(need_items), m_bodyId(0), m_money(0), m_COD(0)
		{}
		/**
		* Creates a new MailDraft object using subject text and content text id.
		*
		* @param subject The subject of the mail.
		* @param itemTextId The id of the body of the mail.
		*/
		MailDraft(String subject, UInt32 itemTextId = 0)
			: m_mailTemplateId(0), m_mailTemplateItemsNeed(false), m_subject(subject), m_bodyId(itemTextId), m_money(0), m_COD(0) {}
		/**
		* Creates a new MailDraft object using subject and contect texts.
		*
		* @param subject The subject of the mail.
		* @param itemText The text of the body of the mail.
		*/
		MailDraft(String subject, String text);
	public:                                                 // Accessors
															/// Returns the template ID used for this MailDraft.
		UInt16 GetMailTemplateId() const { return m_mailTemplateId; }
		/// Returns the subject of this MailDraft.
		String const& GetSubject() const { return m_subject; }
		/// Returns the ID of the text of this MailDraft.
		UInt32 GetBodyId() const { return m_bodyId; }
		/// Returns the amount of money in this MailDraft.
		UInt32 GetMoney() const { return m_money; }
		/// Returns the Cost of delivery of this MailDraft.
		UInt32 GetCOD() const { return m_COD; }
	public:                                                 // modifiers

															// this two modifiers expected to be applied in normal case to blank draft and exclusively, It DON'T must overwrite already set itemTextId, in other cases it will work and with mixed cases but this will be not normal way use.
		MailDraft& SetSubjectAndBodyId(String subject, UInt32 itemTextId) { m_subject = subject; assert(!m_bodyId); m_bodyId = itemTextId; return *this; }
		MailDraft& SetSubjectAndBody(String subject, String text);
		MailDraft& SetMailTemplate(UInt16 mailTemplateId, bool need_items = true) { m_mailTemplateId = mailTemplateId, m_mailTemplateItemsNeed = need_items; return *this; }

		MailDraft& AddItem(GameItem* item);
		/**
		* Modifies the amount of money in a MailDraft.
		*
		* @param money The amount of money included in this MailDraft.
		*/
		MailDraft& SetMoney(UInt32 money) { m_money = money; return *this; }
		/**
		* Modifies the cost of delivery of the MailDraft.
		*
		* @param COD the amount to which the cod should be set.
		*/
		MailDraft& SetCOD(UInt32 COD) { m_COD = COD; return *this; }

		void CloneFrom(MailDraft const& draft);
	public:                                                 // finishers
		void SendReturnToSender(UInt32 sender_acc, UInt64 sender_guid, UInt64 receiver_guid);
		void SendMailTo(MailReceiver const& receiver, MailSender const& sender, MailCheckMask checked = mail_check_mask::None, UInt32 deliver_delay = 0);
	private:
		MailDraft(MailDraft const&);                        // trap decl, no body, mail draft must cloned only explicitly...
		MailDraft& operator=(MailDraft const&);             // trap decl, no body, ...because items clone is high price operation

		void deleteIncludedItems(bool inDB = false);
		bool prepareItems(Player* receiver);                ///< called from SendMailTo for generate mailTemplateBase items

		/// The ID of the template associated with this MailDraft.
		UInt16      m_mailTemplateId;
		/// Boolean specifying whether items are required or not.
		bool        m_mailTemplateItemsNeed;
		/// The subject of the MailDraft.
		String		m_subject;
		/// The ID of the body of the MailDraft.
		UInt32      m_bodyId;
		/// A map of items in this MailDraft.
		MailItemMap m_items;                                ///< Keep the items in a map to avoid duplicate guids (which can happen), store only low part of guid

		/// The amount of money in this MailDraft.
		UInt32 m_money;
		/// The cod amount of this MailDraft.
		UInt32 m_COD;
	};

	/**
	* Structure holding information about an item in the mail.
	*/
	struct MailItemInfo
	{
		UInt32 item_guid;                                       ///< the GUID of the item.
		UInt32 item_template;                                   ///< the ID of the template of the item.
	};

	typedef std::vector<MailItemInfo> MailItemInfoVec;
	/**
	* Structure that holds an actual mail.
	*/
	struct Mail
	{
		/// the ID of the message contained in the mail.
		UInt32 messageID;
		/// the type of the message
		UInt8 messageType;
		/// the stationary used in this mail.
		UInt8 stationery;
		/// the ID of the template this mail is based on.
		UInt16 mailTemplateId;
		/// the LowGUID of the player that sent this mail, or creature low guid, or other id
		UInt32 sender;
		/// the GUID of the player that this mail is sent to.
		UInt64 receiverGuid;
		/// the subject of the mail
		String subject;
		/// The ID of the itemtext.
		UInt32 itemTextId;
		/// flag mark mail that already has items, or already generate none items for template
		bool has_items;
		/// A vector containing Information about the items in this mail.
		MailItemInfoVec items;
		/// A vector containing Information about the items that where already take from this mail.
		std::vector<UInt32> removedItems;
		/// The time at which this mail will expire
		time_t expire_time;
		/// The time at which this mail (was/will be) delivered
		time_t deliver_time;
		/// The amount of money contained in this mail.
		UInt32 money;
		/// The amount of money the receiver has to pay to get this mail.
		UInt32 COD;
		/// The time at which this mail was read.
		UInt32 checked;
		/// The state of this mail.
		MailState state;

		/**
		* Adds an item to the mail.
		* This method adds an item to mail represented by this structure.
		* There is no checking done whether this is a legal action or not; it is up
		* to the caller to make sure there is still room for more items in the mail.
		*
		* @param itemGuidLow      the GUID(low) of the item added to the mail.
		* @param item_template the ID of the template of the item.
		*
		*/
		void AddItem(UInt32 itemGuidLow, UInt32 item_template)
		{
			MailItemInfo mii;
			mii.item_guid = itemGuidLow;
			mii.item_template = item_template;
			items.push_back(mii);
			has_items = true;
		}

		/**
		* Removes an item from the mail.
		* This method removes an item from the mail.
		*
		* @see MailItemInfo
		*
		* @param item_guid        The GUID of the item to be removed.
		* @returns true if the item was removed, or false if no item with that GUID was found.
		*
		*/
		bool RemoveItem(UInt32 item_guid)
		{
			for (auto itr = items.begin(); itr != items.end(); ++itr)
			{
				if (itr->item_guid == item_guid)
				{
					items.erase(itr);
					return true;
				}
			}
			return false;
		}

		/*
		* Checks whether a mail contains items (including case none items generated from template already) or not.
		* HasItems() checks whether the mail contains items or not.
		*
		* @returns true if the mail contains items or template items already generated possible none, false otherwise.
		*
		*/
		bool HasItems() const { return has_items; }

		/*
		* Generate items for template if items not genereated before (receiver has been offline, has_items == false)
		*
		*/
		void prepareTemplateItems(Player* receiver);            ///< called from _LoadMails for generate mailTemplateBase items not generated for offline player
	};

}