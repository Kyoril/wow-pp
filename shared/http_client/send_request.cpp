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
#include "send_request.h"
#include "common/macros.h"

namespace wowpp
{
	namespace net
	{
		namespace http_client
		{
			namespace
			{
				std::string escapePath(const std::string &path)
				{
					std::ostringstream escaped;
					for (const char c : path)
					{
						if (std::isgraph(c) && (c != '%'))
						{
							escaped << c;
						}
						else
						{
							escaped
							        << '%'
							        << std::hex
							        << std::setw(2)
							        << std::setfill('0')
							        << static_cast<unsigned>(c);
						}
					}
					return escaped.str();
				}
			}


			Response sendRequest(
			    const std::string &host,
				NetPort port,
			    const Request &request
			)
			{
				const auto connection = std::make_shared<boost::asio::ip::tcp::iostream>();
				connection->connect(
				    host,
				    boost::lexical_cast<std::string>(port));
				if (!*connection)
				{
					//.error() was introduced in Boost 1.47
					//throw boost::system::system_error(connection->error());
					throw std::runtime_error("Could not connect to " + host);
				}

				*connection << "GET " << escapePath(request.document) << " HTTP/1.0\r\n";
				*connection << "Host: " << request.host << "\r\n";
				*connection << "Accept: */*\r\n";
				*connection << "Connection: close\r\n";
				*connection << "\r\n";

				std::string responseVersion;
				unsigned status;
				std::string statusMessage;

				*connection
				        >> responseVersion
				        >> status;
				std::getline(*connection, statusMessage);

				std::map<std::string, std::string> headers;
				boost::optional<boost::uintmax_t> bodySize(0);
				bodySize.reset();

				{
					std::string line;
					while (std::getline(*connection, line) && (line != "\r"))
					{
						const auto endOfKey = std::find(line.begin(), line.end(), ':');
						auto beginOfValue = (endOfKey + 1);
						if (endOfKey == line.end() ||
						        beginOfValue == line.end())
						{
							throw std::runtime_error("Invalid HTTP response header");
						}

						if (*beginOfValue == ' ')
						{
							++beginOfValue;
						}

						auto endOfValue = line.end();
						if (endOfValue[-1] == '\r')
						{
							--endOfValue;
						}

						std::string key(line.begin(), endOfKey);
						std::string value(beginOfValue, endOfValue);

						static const std::string ContentLength = "Content-Length";
						if (boost::iequals(key, ContentLength))
						{
							bodySize = boost::lexical_cast<boost::uintmax_t>(value);
						}

						headers.insert(std::make_pair(
						                   std::move(key),
						                   std::move(value)
						               ));
					}
				}

				auto body = boost::iostreams::restrict(*connection, 0, bodySize ? static_cast<boost::iostreams::stream_offset>(*bodySize) : -1L);
				std::unique_ptr<std::istream> bodyStream(new boost::iostreams::stream<decltype(body)>(
				            body
				        ));

				Response response(
				    status,
				    bodySize,
				    std::move(bodyStream),
				    connection
				);
				response.headers = std::move(headers);
				return response;
			}
		}
	}
}
