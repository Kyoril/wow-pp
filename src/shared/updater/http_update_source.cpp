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
#include "http_update_source.h"
#include "virtual_directory/path.h"
#include "http_client/send_request.h"

namespace wowpp
{
	namespace updating
	{
		HTTPUpdateSource::HTTPUpdateSource(
		    std::string host,
			NetPort port,
		    std::string path
		)
			: m_host(std::move(host))
			, m_port(port)
			, m_path(std::move(path))
		{
		}

		UpdateSourceFile HTTPUpdateSource::readFile(
		    const std::string &path
		)
		{
			net::http_client::Request request;
			request.host = m_host;
			request.document = m_path;
			virtual_dir::appendPath(request.document, path);

			auto response = net::http_client::sendRequest(
			                    m_host,
			                    m_port,
			                    request);

			if (response.status != net::http_client::Response::Ok)
			{
				throw std::runtime_error(
				    path + ": HTTP response " +
				    boost::lexical_cast<std::string>(response.status));
			}

			return UpdateSourceFile(
			           response.getInternalData(),
			           std::move(response.body),
			           response.bodySize
			       );
		}
	}
}
