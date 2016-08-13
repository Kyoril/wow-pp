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
#include "response.h"

namespace wowpp
{
	namespace net
	{
		namespace http_client
		{
			Response::Response()
				: status(0)
				, bodySize(0)
			{
			}

			Response::Response(
			    unsigned status,
			    boost::optional<boost::uintmax_t> bodySize,
			    std::unique_ptr<std::istream> body,
			    boost::any internalData)
				: status(status)
				, bodySize(bodySize)
				, body(std::move(body))
				, m_internalData(std::move(internalData))
			{
			}

			Response::Response(Response  &&other)
			{
				swap(other);
			}

			Response::~Response()
			{
				//body must be destroyed before m_internalData
				//because body may depend on it
				body.reset();
			}

			Response &Response::operator = (Response && other)
			{
				swap(other);
				return *this;
			}

			void Response::swap(Response &other)
			{
				using std::swap;
				using boost::swap;

				swap(status, other.status);
				swap(bodySize, other.bodySize);
				swap(body, other.body);
				swap(headers, other.headers);
				swap(m_internalData, other.m_internalData);
			}

			const boost::any &Response::getInternalData() const
			{
				return m_internalData;
			}
		}
	}
}
