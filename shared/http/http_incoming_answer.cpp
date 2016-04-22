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
#include "http_incoming_answer.h"
#include "common/constants.h"

namespace wowpp
{
	namespace net
	{
		namespace http
		{
			IncomingAnswer::IncomingAnswer()
				: m_status(OK)
				, m_content("")
			{
			}

			IncomingAnswer::Status IncomingAnswer::getStatus() const
			{
				return m_status;
			}

			const String &IncomingAnswer::getStatusText() const
			{
				return m_statusText;
			}

			const String &IncomingAnswer::getContent() const
			{
				return m_content;
			}

			const IncomingAnswer::Headers &IncomingAnswer::getHeaders() const
			{
				return m_headers;
			}

			static bool isWhitespace(char c)
			{
				return (c <= ' ');
			}

			static void skipWhitespace(const char *&pos, const char *end)
			{
				while (
				    pos != end &&
				    isWhitespace(*pos))
				{
					++pos;
				}
			}

			static void parseThing(const char *&pos, const char *end, String &dest)
			{
				assert(dest.empty());

				while (pos != end &&
				        !isWhitespace(*pos))
				{
					dest += *pos;
					++pos;
				}
			}

			static bool skipCharacter(const char *&pos, const char *end, char c)
			{
				if (pos != end &&
				        *pos == c)
				{
					++pos;
					return true;
				}

				return false;
			}

			static bool skipEndOfLine(const char *&pos, const char *end)
			{
				const char *savePos = pos;

				if (
				    skipCharacter(savePos, end, '\r') &&
				    skipCharacter(savePos, end, '\n'))
				{
					pos = savePos;
					return true;
				}

				return false;
			}

			static void readUntil(const char *&pos, const char *end, String &value, char until)
			{
				assert(value.empty());

				while (pos != end &&
				        *pos != until)
				{
					value += *pos;
					++pos;
				}
			}

			ReceiveState IncomingAnswer::start(IncomingAnswer &packet, io::MemorySource &source)
			{
				const char *const begin = source.getBegin();
				const char *const end = source.getEnd();
				const char *pos = begin;

				//request line
				{
					String method;
					parseThing(pos, end, method);

					if (method.empty())
					{
						return receive_state::Incomplete;
					}
					else if (method != "HTTP/1.1")
					{
						return receive_state::Malformed;
					}
				}

				skipWhitespace(pos, end);

				String code;
				parseThing(pos, end, code);
				if (code.empty())
				{
					return receive_state::Incomplete;
				}

				const auto iCodeNumber = atoi(code.c_str());
				switch (iCodeNumber)
				{
				case 200:
					{
						packet.m_status = OK;
						break;
					}

				case 400:
					{
						packet.m_status = BadRequest;
						break;
					}

				case 401:
					{
						packet.m_status = Unauthorized;
						break;
					}

				case 403:
					{
						packet.m_status = Forbidden;
						break;
					}

				case 404:
					{
						packet.m_status = NotFound;
						break;
					}

				case 500:
					{
						packet.m_status = InternalServerError;
						break;
					}

				case 503:
					{
						packet.m_status = ServiceUnavailable;
						break;
					}

				default:
					{
						return receive_state::Malformed;
					}
				}

				skipWhitespace(pos, end);

				{
					parseThing(pos, end, packet.m_statusText);
					if (packet.m_statusText.empty())
					{
						return receive_state::Incomplete;
					}
				}

				if (!skipEndOfLine(pos, end))
				{
					return receive_state::Malformed;
				}

				//headers
				while (!skipEndOfLine(pos, end))
				{
					String name;

					readUntil(pos, end, name, ':');
					if (name.empty())
					{
						break;
					}

					if (!skipCharacter(pos, end, ':'))
					{
						return receive_state::Malformed;
					}

					skipWhitespace(pos, end);

					String value;
					readUntil(pos, end, value, '\r');

					if (!skipEndOfLine(pos, end))
					{
						return receive_state::Malformed;
					}

					packet.m_headers.insert(std::make_pair(name, value));
				}

				// Skip header
				source.skip(pos - begin);

				// Create buffer
				std::vector<char> buffer(end - pos);
				source.read(buffer.data(), end - pos);

				// Set content
				packet.m_content = String(buffer.begin(), buffer.end());

				source.skip(pos - begin);
				return receive_state::Complete;
			}
		}
	}
}
