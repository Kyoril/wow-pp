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
#include "update_url.h"

namespace wowpp
{
	namespace updating
	{
		UpdateURL::UpdateURL(const std::string &url)
			: port(0)
		{
			if (boost::algorithm::starts_with(url, "http://"))
			{
				scheme = UP_HTTP;
				const auto beginOfHost = url.begin() + 7;
				const auto slash = std::find(beginOfHost, url.end(), '/');
				const auto colon = std::find(beginOfHost, slash, ':');
				if (colon != slash)
				{
					port = boost::lexical_cast<NetPort>(
					           std::string(colon + 1, slash));
				}
				host.assign(beginOfHost, colon);
				path.assign(slash, url.end());

				if (host.empty())
				{
					throw std::invalid_argument("Invalid URL: Host expected");
				}

				if (path.empty())
				{
					path = "/";
				}
			}
			else
			{
				scheme = UP_FileSystem;
				path = url;
			}
		}

		UpdateURL::UpdateURL(
		    UpdateURLScheme scheme,
		    std::string host,
			NetPort port,
		    std::string path)
			: scheme(scheme)
			, host(std::move(host))
			, port(port)
			, path(std::move(path))
		{
		}


		bool operator == (const UpdateURL &left, const UpdateURL &right)
		{
			return
			    (left.scheme == right.scheme) &&
			    (left.host == right.host) &&
			    (left.port == right.port) &&
			    (left.path == right.path);
		}
	}
}
