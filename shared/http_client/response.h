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

namespace wowpp
{
	namespace net
	{
		namespace http_client
		{
			struct Response
			{
				enum
				{
				    Ok = 200,
				    NotFound = 404
				};

				unsigned status;
				std::map<std::string, std::string> headers;
				boost::optional<boost::uintmax_t> bodySize;
				std::unique_ptr<std::istream> body;


				Response();
				explicit Response(
				    unsigned status,
				    boost::optional<boost::uintmax_t> bodySize,
				    std::unique_ptr<std::istream> body,
				    boost::any internalData);
				Response(Response  &&other);
				~Response();
				Response &operator = (Response && other);
				void swap(Response &other);
				const boost::any &getInternalData() const;

			private:

				boost::any m_internalData;
			};


			void swap(Response &left, Response &right);
		}
	}
}
