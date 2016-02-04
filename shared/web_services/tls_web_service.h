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

#include "http/http_incoming_request.h"
#include "http/http_outgoing_answer.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <functional>
#include <memory>

namespace wowpp
{
	namespace web
	{
		class TLSWebService
		{
		public:

			typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> Socket;
			typedef std::function<void(net::http::IncomingRequest &,
			                           net::http::OutgoingAnswer &)> RequestHandler;


			explicit TLSWebService(
			    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor,
			    std::unique_ptr<boost::asio::ssl::context> ssl,
			    RequestHandler handleRequest
			);

		private:

			const std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;
			const std::unique_ptr<boost::asio::ssl::context> m_ssl;
			const RequestHandler m_handleRequest;


			void beginAccept();
			void onAccept(boost::system::error_code error,
			              std::shared_ptr<Socket> connection);
		};


		std::unique_ptr<boost::asio::ssl::context> createSSLContext(
		    const std::string &caFileName,
		    const std::string &privateKeyFileName,
		    const std::string &password);
	}
}
