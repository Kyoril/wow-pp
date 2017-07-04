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
		, m_mailId(0)
		, m_messageType(mail::message_type::Normal)
		, m_stationery(mail::stationery::Default)
		, m_checkMasks(mail::check_mask::None)
	{
	}

	Mail::Mail(
		const UInt64 & senderGuid,
		std::vector<std::pair<UInt32, ItemData>> & items,
		const MailData & mailInfo,
		UInt32 checkMasks,
		UInt8 messageType,
		UInt32 stationery)
		: m_senderGuid(senderGuid)
		, m_subject(mailInfo.subject)
		, m_body(mailInfo.body)
		, m_items(std::move(items))
		, m_money(mailInfo.money)
		, m_COD(mailInfo.COD)
		, m_mailId(0)
		, m_messageType(messageType)
		, m_stationery(stationery)
		, m_checkMasks(checkMasks)
	{
	}

	io::Writer & operator<<(io::Writer & w, const MailResult & out_mailResult)
	{
		w.writePOD(out_mailResult);
		return w;
	}

	io::Reader & operator >> (io::Reader & r, MailResult & mailResult)
	{
		r.readPOD(mailResult);
		return r;
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
			<< io::write<NetUInt32>(object.m_mailId)
			<< io::write<NetUInt8>(object.m_messageType)
			<< io::write<NetUInt32>(object.m_stationery)
			<< io::write<NetUInt32>(object.m_checkMasks)
			<< io::write<NetUInt32>(object.m_items.size())
			;

		for (auto &item : object.m_items)
		{
			w
				<< item.first
				<< item.second
				;
		}

		return w;
	}

	io::Reader & operator >> (io::Reader & r, Mail & object)
	{
		size_t itemsCount;

		r
			>> io::read<NetUInt64>(object.m_senderGuid)
			>> io::read_container<NetUInt8>(object.m_subject)
			>> io::read_container<NetUInt8>(object.m_body)
			>> io::read<NetUInt32>(object.m_money)
			>> io::read<NetUInt32>(object.m_COD)
			>> io::read<NetUInt32>(object.m_mailId)
			>> io::read<NetUInt8>(object.m_messageType)
			>> io::read<NetUInt32>(object.m_stationery)
			>> io::read<NetUInt32>(object.m_checkMasks)
			>> io::read<NetUInt32>(itemsCount)
			;		

		for (UInt32 i = 0; i < itemsCount; ++i)
		{
			UInt32 guid;
			ItemData item;
			r
				>> guid
				>> item
				;
			object.m_items.push_back(std::make_pair(guid, item));
		}

		return r;
	}
}
