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

#include "http_outgoing_answer.h"
#include "common/constants.h"
#include <sstream>

namespace wowpp
{
	namespace net
	{
		namespace http
		{
			OutgoingAnswer::OutgoingAnswer(io::ISink &dest)
				: m_dest(dest)
				, m_status(OK)
			{
			}

			void OutgoingAnswer::setStatus(Status status)
			{
				m_status = status;
			}

			void OutgoingAnswer::addHeader(const String &name, const String &value)
			{
				m_additionalHeaders += name;
				m_additionalHeaders += ": ";
				m_additionalHeaders += value;
				m_additionalHeaders += "\r\n";
			}

			void OutgoingAnswer::finish()
			{
				writeHeaders();
				m_dest.write("\r\n", 2);
			}

			void OutgoingAnswer::finishWithContent(const String &type, const char *content, std::size_t size)
			{
				writeHeaders();

				std::ostringstream sstr;

				sstr << "Content-Type: " << type << "\r\n";
				sstr << "Content-Length: " << size << "\r\n";
				sstr << "\r\n";

				sstr.write(content, static_cast<std::streamsize>(size));

				const std::string str = sstr.str();
				m_dest.write(str.data(), str.size());
			}


			void OutgoingAnswer::makeAnswer(io::ISink &dest, const String &type, const char *content, std::size_t size)
			{
				OutgoingAnswer packet(dest);
				packet.setStatus(OK);
				packet.finishWithContent(type, content, size);
			}

			void OutgoingAnswer::makeHtmlAnswer(io::ISink &dest, const char *content, std::size_t size)
			{
				makeAnswer(dest, "text/html", content, size);
			}


			void OutgoingAnswer::writeHeaders()
			{
				std::ostringstream sstr;

				static const char *const StatusStrings[StatusCount_] =
				{
					"200 OK",

					"400 Bad Request",
					"401 Unauthorized",
					"403 Forbidden",
					"404 Not Found",

					"500 Internal Server Error",
					"503 Service Unavailable"
				};

				sstr << "HTTP/1.1 " << StatusStrings[m_status] << "\r\n";
				sstr << m_additionalHeaders;

				const std::string str = sstr.str();
				m_dest.write(str.data(), str.size());
			}


			void respondUnauthorized(OutgoingAnswer &response,
			                         const std::string &realmName)
			{
				response.setStatus(OutgoingAnswer::Unauthorized);
				response.addHeader("WWW-Authenticate", "Basic realm=\"" + realmName + "\"");
				response.finishWithContent("text/html", "", 0);
			}
		}
	}
}
