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
#include "binary_io/reader.h"
#include "binary_io/memory_source.h"
#include "binary_io/sink.h"

namespace wowpp
{
	namespace net
	{
		namespace http
		{
			struct OutgoingAnswer
			{
				enum Status
				{
				    OK,

				    BadRequest,
				    Unauthorized,
				    Forbidden,
				    NotFound,

				    InternalServerError,
				    ServiceUnavailable,

				    StatusCount_
				};


				explicit OutgoingAnswer(io::ISink &dest);
				void setStatus(Status status);
				void addHeader(const String &name, const String &value);
				void finish();
				void finishWithContent(const String &type, const char *content, std::size_t size);

				static void makeAnswer(io::ISink &dest, const String &type, const char *content, std::size_t size);
				static void makeHtmlAnswer(io::ISink &dest, const char *content, std::size_t size);

			private:

				io::ISink &m_dest;
				Status m_status;
				String m_additionalHeaders;


				void writeHeaders();
			};


			void respondUnauthorized(OutgoingAnswer &response,
			                         const std::string &realmName);
		}
	}
}
