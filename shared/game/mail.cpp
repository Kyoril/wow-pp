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

#include "pch.h"
#include "mail.h"

namespace wowpp
{
	Mail::Mail()
		: m_senderGuid(0)
		, m_money(0)
		, m_COD(0)
		, m_read(0)
	{
	}

	Mail::Mail(
		const UInt64 & senderGuid,
		ItemVector & itemVector,
		const MailData & mailInfo,
		bool read)
		: m_senderGuid(senderGuid)
		, m_subject(mailInfo.subject)
		, m_body(mailInfo.body)
		, m_items(std::move(itemVector))
		, m_money(mailInfo.money)
		, m_COD(mailInfo.COD)
		, m_read(read)
	{
	}

	io::Reader & operator >> (io::Reader & r, MailData & out_mail)
	{
		r
			>> io::read_string(out_mail.receiver)
			>> io::read_string(out_mail.subject)
			>> io::read_string(out_mail.body)
			>> io::skip(sizeof(UInt32))
			>> io::skip(sizeof(UInt32));

		// Read items on mail list, 12 being the client limit
		r >> io::read<UInt8>(out_mail.itemsCount);
		if (out_mail.itemsCount > 12)
		{
			// TODO Error message
			return r;
		}

		// Read itemsGuid, skipping item slot in mail
		for (UInt8 i = 0; i < out_mail.itemsCount; ++i)
		{
			r
				>> io::skip(sizeof(UInt8))
				>> io::read<NetObjectGuid>(out_mail.itemsGuids[i]);
		}

		r
			>> io::read<NetUInt32>(out_mail.money)
			>> io::read<NetUInt32>(out_mail.COD)
			>> io::skip(sizeof(UInt64))
			>> io::skip(sizeof(UInt8));

		return r;
	}

	io::Writer &operator<<(io::Writer &w, const Mail &object)
	{
		w
			<< io::write<NetUInt64>(object.m_senderGuid)
			<< io::write_dynamic_range<NetUInt8>(object.m_subject)
			<< io::write_dynamic_range<NetUInt8>(object.m_body)
			<< io::write<NetUInt32>(object.m_money)
			<< io::write<NetUInt32>(object.m_COD)
			<< io::write<NetUInt8>(object.m_read ? 1 : 0)
			;
		// TODO items

		return w;
	}

	io::Reader & operator >> (io::Reader & r, Mail & object)
	{
		r
			>> io::read<NetUInt64>(object.m_senderGuid)
			>> io::read_container<NetUInt8>(object.m_subject)
			>> io::read_container<NetUInt8>(object.m_body)
			>> io::read<NetUInt32>(object.m_money)
			>> io::read<NetUInt32>(object.m_COD)
			>> io::read<NetUInt8>(object.m_read)
			;

		return r;
	}
}
