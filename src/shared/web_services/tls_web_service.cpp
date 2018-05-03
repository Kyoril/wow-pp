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
#include "tls_web_service.h"
#include "common/make_unique.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace web
	{
		namespace
		{
			struct TLSWebClient
			{
			};
		}

		TLSWebService::TLSWebService(
		    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor,
		    std::unique_ptr<boost::asio::ssl::context> ssl,
		    RequestHandler handleRequest
		)
			: m_acceptor(std::move(acceptor))
			, m_ssl(std::move(ssl))
			, m_handleRequest(std::move(handleRequest))
		{
			assert(m_acceptor);
			assert(m_ssl);
			assert(m_handleRequest);
			beginAccept();
		}

		void TLSWebService::beginAccept()
		{
			const auto connection =
			    std::make_shared<Socket>(m_acceptor->get_io_service(), *m_ssl);

			auto acceptHandler = std::bind(&TLSWebService::onAccept,
			                               this,
			                               std::placeholders::_1,
			                               connection);

			m_acceptor->async_accept(connection->lowest_layer(),
			                         std::move(acceptHandler));
		}

		void TLSWebService::onAccept(boost::system::error_code error,
		                             std::shared_ptr<Socket> connection)
		{
			assert(connection);
			DLOG("Incoming TLS client");

			if (error)
			{
				return;
			}

			beginAccept();
		}

		std::unique_ptr<boost::asio::ssl::context> createSSLContext(
		    const std::string &caFileName,
		    const std::string &privateKeyFileName,
		    const std::string &password)
		{
			auto context = make_unique<boost::asio::ssl::context>(
			                   boost::asio::ssl::context::sslv23);

			context->set_options(
			    boost::asio::ssl::context::default_workarounds |
			    boost::asio::ssl::context::no_sslv2 |
			    boost::asio::ssl::context::single_dh_use);
			context->set_password_callback(
			    [password]
			    (std::size_t, boost::asio::ssl::context_base::password_purpose)
			{
				return password;
			});
			context->use_certificate_chain_file(caFileName);
			context->use_private_key_file(privateKeyFileName,
			                              boost::asio::ssl::context::pem);
			//	context->use_tmp_dh_file("dh512.pem");
			return context;
		}
	}
}
