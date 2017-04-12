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
#include "game_item.h"

namespace io
{
	class Writer;
	class Reader;
}

namespace wowpp
{
	namespace mail
	{
		namespace stationery
		{
			enum Type
			{
				Unknown = 1,
				Default = 41,
				GM = 61,
				Auction = 62,
				Val = 64,
				Chr = 65
			};
		}

		typedef stationery::Type Stationery;

		namespace check_mask
		{
			enum Type
			{
				None = 0x00,
				Read = 0x01,
				Returned = 0x02,
				Copied = 0x04,
				PaymentCOD = 0x08,
				HasBody = 0x10
			};
		}

		typedef check_mask::Type CheckMask;

		namespace message_type
		{
			enum Type
			{
				Normal = 0,
				Auction = 2,
				Creature = 3,
				GameObject = 4,
				Item = 5
			};
		}

		typedef message_type::Type MessageType;

		namespace response_type
		{
			enum Type
			{
				Send = 0,
				MoneyTaken = 1,
				ItemTaken = 2,
				ReturnedToSender = 3,
				Deleted = 4,
				MadePermanent = 5
			};
		}

		typedef response_type::Type ResponseType;

		namespace response_result
		{
			enum Type
			{
				Ok = 0,
				Equip = 1,
				CannotSendToSelf = 2,
				NotEnoughMoney = 3,
				RecipientNotFound = 4,
				NotYourTeam = 5,
				Internal = 6,
				DisabledForTrialAcc = 14,
				RecipientCapReached = 15,
				CannotSendWrappedCOD = 16,
				AndChatSuspended = 17,
				TooManyAttachments = 18,
				AttachmentInvalid = 19,
			};
		}

		typedef response_result::Type ResponseResult;
	}

	typedef std::vector<std::shared_ptr<GameItem>> ItemVector;

	struct MailResult final
	{
		UInt32 mailId;
		mail::ResponseType mailAction;
		mail::ResponseResult mailError;
		UInt32 equipError;
		// guid might be 64b, will have to test
		UInt32 itemGuid;
		UInt32 itemsCount;

		MailResult(UInt32 mId, mail::ResponseType mAction, mail::ResponseResult mError, UInt32 eqError = 0, UInt32 itGuid = 0, UInt32 itsCount = 0)
			: mailId(mId)
			, mailAction(mAction)
			, mailError(mError)
			, equipError(eqError)
			, itemGuid(itGuid)
			, itemsCount(itsCount)
		{
		}
	};
	
	io::Writer &operator << (io::Writer &w, const MailResult &out_mailResult);
	io::Reader &operator >> (io::Reader &r, MailResult &mailResult);

	struct MailData final
	{
		String receiver, subject, body;
		UInt32 money, COD;
		UInt8 itemsCount;
		std::array<ObjectGuid, 12> itemsGuids;

		MailData()
			: money(0)
			, COD(0)
			, itemsCount(0)
		{
		}
	};

	io::Reader &operator >> (io::Reader &r, MailData &out_mailData);

	class Mail
	{
		friend io::Writer &operator << (io::Writer &w, const Mail &object);
		friend io::Reader &operator >> (io::Reader &r, Mail &object);

	public:
		Mail();
		explicit Mail(
			const UInt64 &senderGuid,
			ItemVector &itemVector,
			const MailData &mailInfo,
			bool read = false);
		///
		UInt64 getSenderGuid() const { return m_senderGuid; }
		///
		String getSubject() const { return m_subject; }
		///
		String getBody() const { return m_body; }
		///
		ItemVector &getItems() { return m_items; }
		///
		UInt32 getCOD() const { return m_COD; }
		///
		UInt32 getMoney() const { return m_money; }
		///
		bool isRead() const { return m_read; }
		///
		UInt32 getMailId() const { return m_mailId; }
		///
		void setSenderGuid(UInt64 senderGuid) { 
			m_senderGuid = senderGuid; 
		}
		///
		void setSubject(String &subject) {
			m_subject = subject;
		}
		///
		void setBody(String &body) {
			m_body = body;
		}
		///
		void setMoney(UInt32 money) {
			m_money = money;
		}
		///
		void setCOD(UInt32 COD) {
			m_COD = COD;
		}
		///
		void setReadState(bool read) {
			m_read = read;
		}
		///
		void setMailId(UInt32 mailId) {
			m_mailId = mailId;
		}


	private:
		UInt64 m_senderGuid;
		String m_subject, m_body;
		// TODO change items
		ItemVector m_items;
		UInt32 m_money, m_COD;
		bool m_read;
		UInt32 m_mailId;
		/*
		UInt8 m_messageType;
		UInt32 m_stationery;
		UInt32 m_checkMasks;
		*/
	};

	io::Writer &operator << (io::Writer &w, const Mail &object);
	io::Reader &operator >> (io::Reader &r, Mail &object);
}
