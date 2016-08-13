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
#include "http_incoming_request.h"
#include "common/constants.h"
#include "base64/base64.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace net
	{
		namespace http
		{
			IncomingRequest::IncomingRequest()
				: m_type(Unknown_)
			{
			}

			IncomingRequest::Type IncomingRequest::getType() const
			{
				return m_type;
			}

			const String &IncomingRequest::getPath() const
			{
				return m_path;
			}

			const String &IncomingRequest::getHost() const
			{
				return m_host;
			}

			const String &IncomingRequest::getPostData() const
			{
				return m_postData;
			}

			const IncomingRequest::Headers &IncomingRequest::getHeaders() const
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

			ReceiveState IncomingRequest::start(IncomingRequest &packet, io::MemorySource &source)
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
					else if (method == "HEAD")
					{
						packet.m_type = Head;
					}
					else if (method == "GET")
					{
						packet.m_type = Get;
					}
					else if (method == "POST")
					{
						packet.m_type = Post;
					}
					else if (method == "OPTIONS")
					{
						packet.m_type = Options;
					}
					else
					{
						return receive_state::Malformed;
					}
				}

				skipWhitespace(pos, end);
				parseThing(pos, end, packet.m_path);
				if (packet.m_path.empty())
				{
					return receive_state::Incomplete;
				}

				skipWhitespace(pos, end);

				{
					String version;
					parseThing(pos, end, version);
					if (version.empty())
					{
						return receive_state::Incomplete;
					}
				}

				if (!skipEndOfLine(pos, end))
				{
					return receive_state::Malformed;
				}

				// We need at least another end of line
				if (pos == end)
				{
					return receive_state::Incomplete;
				}

				//headers
				packet.m_headers.clear();
				size_t estimatedContentLength = 0;
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
						WLOG("Malformed for header " << name << ": " << pos);
						return receive_state::Malformed;
					}

					skipWhitespace(pos, end);

					String value;
					readUntil(pos, end, value, '\r');

					if (!skipEndOfLine(pos, end))
					{
						WLOG("Malformed for header " << name << ": " << value << " - " << pos);
						return receive_state::Malformed;
					}

					if (name == "Content-Length")
					{
						estimatedContentLength = atoi(value.c_str());
					}

					packet.m_headers.insert(std::make_pair(name, value));
				}

				if (estimatedContentLength > 0)
				{
					if (end <= pos ||
					        size_t(end - pos) < estimatedContentLength)
					{
						return receive_state::Incomplete;
					}
				}
				else if (estimatedContentLength == 0)
				{
					source.skip(pos - begin);
					return receive_state::Complete;
				}

				parseThing(pos, end, packet.m_postData);
				source.skip(pos - begin);

				return receive_state::Complete;
			}

			namespace
			{
				std::pair<std::string, std::string> parseHTTPAuthorization(
				    const std::string &encoded)
				{
					const std::pair<std::string, std::string> errorResult;

					//the beginning is expected to be "Basic "
					static const std::string Begin = "Basic ";
					if (encoded.size() < Begin.size())
					{
						return errorResult;
					}

					if (!std::equal(Begin.begin(), Begin.end(), encoded.begin()))
					{
						return errorResult;
					}

					const auto base64Begin = encoded.begin() + Begin.size();
					const auto base64End = encoded.end();

					//TODO: save memory allocation
					const auto concatenated = base64_decode(std::string(base64Begin, base64End));
					const auto colon = std::find(begin(concatenated), end(concatenated), ':');
					const auto afterColon = (colon == end(concatenated) ? colon : (colon + 1));

					return std::make_pair(
					           std::string(begin(concatenated), colon),
					           std::string(afterColon, end(concatenated)));
				}
			}

			bool authorize(
			    const IncomingRequest &request,
			    const std::function<bool (const std::string &, const std::string &)> &checkCredentials)
			{
				const auto &headers = request.getHeaders();
				const auto authorization = headers.find("Authorization");
				if (authorization == headers.end())
				{
					return false;
				}

				const auto &encodedCredentials = authorization->second;
				const auto credentials = parseHTTPAuthorization(encodedCredentials);

				const auto &name = credentials.first;
				const auto &password = credentials.second;

				return checkCredentials(name, password);
			}
		}
	}
}
