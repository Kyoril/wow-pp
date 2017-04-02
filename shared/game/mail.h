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
	typedef std::vector<std::shared_ptr<GameItem>> ItemVector;

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


	private:
		UInt64 m_senderGuid;
		String m_subject, m_body;
		// TODO change items
		ItemVector m_items;
		UInt32 m_money, m_COD;
		bool m_read;
	};

	io::Writer &operator << (io::Writer &w, const Mail &object);
	io::Reader &operator >> (io::Reader &r, Mail &object);
}
