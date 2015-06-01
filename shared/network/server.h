//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#pragma once

#include "common/typedefs.h"
#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <memory>
#include <cassert>

namespace wowpp
{
	/// Exception type throwed if binding to a specific port failed.
	struct BindFailedException : public std::exception
	{
	};


	/// Basic server class which manages connection.
	template<typename C>
	class Server : boost::noncopyable
	{
	public:

		typedef C Connection;
		typedef boost::asio::ip::tcp::acceptor AcceptorType;
		typedef boost::signals2::signal<void(const std::shared_ptr<Connection> &)> ConnectionSignal;
		typedef std::function<std::shared_ptr<Connection>(boost::asio::io_service &)> ConnectionFactory;

	public:

		/// Default constructor which does nothing.
		Server() { }
		virtual ~Server() { }

		/// Initializes a new server instance and binds it to a specific port.
		Server(boost::asio::io_service &IOService, NetPort Port, ConnectionFactory CreateConnection)
			: mCreateConn(std::move(CreateConnection))
            , mState(new State(std::unique_ptr<AcceptorType>(new AcceptorType(IOService))))
		{
			assert(mCreateConn);

			try
			{
				mState->Acceptor->open(boost::asio::ip::tcp::v4());
#if !defined(WIN32) && !defined(_WIN32)
				mState->Acceptor->set_option(typename AcceptorType::reuse_address(true));
#endif
				mState->Acceptor->bind(boost::asio::ip::tcp::endpoint(
					boost::asio::ip::tcp::v4(),
					static_cast<UInt16>(Port)));
				mState->Acceptor->listen(16);
			}
			catch (const boost::system::system_error &)
			{
				throw BindFailedException();
			}
		}
		/// Swap constructor.
		Server(Server &&Other)
		{
			swap(Other);
		}
		/// Swap operator overload.
		Server &operator =(Server &&Other)
		{
			swap(Other);
			return *this;
		}
		/// Swaps contents of one server instance with another server instance.
		void swap(Server &Other)
		{
			std::swap(mCreateConn, Other.mCreateConn);
			std::swap(mState, Other.mState);
		}
		/// Gets the signal which is fired if a new connection was accepted.
		ConnectionSignal &connected()
		{
			return mState->Connected;
		}
		/// Starts waiting for incoming connections to accept.
		void startAccept()
		{
			assert(mState);
			const std::shared_ptr<Connection> Conn = mCreateConn(mState->Acceptor->get_io_service());

			mState->Acceptor->async_accept(
				Conn->getSocket().lowest_layer(),
				std::bind(&Server<C>::Accepted, this, Conn, std::placeholders::_1));
		}

	private:

		struct State
		{
			std::unique_ptr<AcceptorType> Acceptor;
			ConnectionSignal Connected;

			explicit State(std::unique_ptr<AcceptorType> Acceptor_)
				: Acceptor(std::move(Acceptor_))
			{
			}
		};

		ConnectionFactory mCreateConn;
		std::unique_ptr<State> mState;

		void Accepted(std::shared_ptr<Connection> Conn, const boost::system::error_code &Error)
		{
			assert(Conn);
			assert(mState);

			if (Error)
			{
				//TODO
				return;
			}

			mState->Connected(Conn);
			startAccept();
		}
	};
}
