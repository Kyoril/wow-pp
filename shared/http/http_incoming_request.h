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
#include "network/receive_state.h"
#include "binary_io/reader.h"
#include "binary_io/memory_source.h"
#include <functional>
#include <map>

namespace wowpp
{
	namespace net
	{
		namespace http
		{
			struct IncomingRequest : io::Reader
			{
				enum Type
				{
				    Head,
				    Get,
				    Post,
					Options,

				    Unknown_
				};


				typedef std::map<String, String> Headers;

				IncomingRequest();
				Type getType() const;
				const String &getPath() const;
				const String &getHost() const;
				const Headers &getHeaders() const;

				static ReceiveState start(IncomingRequest &packet, io::MemorySource &source);

			private:

				Type m_type;
				String m_path;
				String m_host;
				Headers m_headers;
			};


			bool authorize(
			    const IncomingRequest &request,
			    const std::function<bool (const std::string &, const std::string &)> &checkCredentials);
		}
	}
}
