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
#include "open_source_from_url.h"
#include "update_source.h"
#include "update_url.h"
#include "file_system_update_source.h"
#include "http_update_source.h"

namespace wowpp
{
	namespace updating
	{
		std::unique_ptr<IUpdateSource> openSourceFromUrl(
		    const UpdateURL &url)
		{
			std::unique_ptr<IUpdateSource> source;

			switch (url.scheme)
			{
			case UP_FileSystem:
				source.reset(new FileSystemUpdateSource(url.path));
				break;

			case UP_HTTP:
				source.reset(new HTTPUpdateSource(
				                 url.host,
				                 static_cast<NetPort>(url.port ? url.port : 80),
				                 url.path));
				break;

			default:
				throw std::runtime_error("Unknown URL type");
			}

			return source;
		}
	}
}
