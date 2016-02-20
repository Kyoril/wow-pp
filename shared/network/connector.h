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
#include "common/weak_ptr_function.h"
#include "connection.h"

namespace wowpp
{
	template <class P>
	struct IConnectorListener : IConnectionListener<P>
	{
		typedef P Protocol;

		virtual bool connectionEstablished(bool success) = 0;
	};


	template <class P, class MySocket = boost::asio::ip::tcp::socket>
	class Connector : public Connection<P, MySocket>
	{
	public:

		typedef Connection<P> Super;
		typedef P Protocol;
		typedef IConnectorListener<P> Listener;
		typedef MySocket Socket;

	public:

		explicit Connector(std::unique_ptr<Socket> socket, Listener *listener);
		~Connector();
		virtual void setListener(Listener &listener);
		Listener *getListener() const;
		void connect(const String &host, NetPort port, Listener &listener,
		             boost::asio::io_service &ioService);

		static std::shared_ptr<Connector> create(boost::asio::io_service &service, Listener *listener = nullptr);

	private:

		std::unique_ptr<boost::asio::ip::tcp::resolver> m_resolver;
		NetPort m_port;


		void handleResolve(const boost::system::error_code &error, boost::asio::ip::tcp::resolver::iterator iterator);
		void beginConnect(boost::asio::ip::tcp::resolver::iterator iterator);
		void handleConnect(const boost::system::error_code &error, boost::asio::ip::tcp::resolver::iterator iterator);
	};


	template <class P, class MySocket>
	Connector<P, MySocket>::Connector(
	    std::unique_ptr<Socket> socket,
	    Listener *listener)
		: Connection<P, MySocket>(std::move(socket), listener)
		, m_port(0)
	{
	}

	template <class P, class MySocket>
	Connector<P, MySocket>::~Connector()
	{
	}

	template <class P, class MySocket>
	void Connector<P, MySocket>::setListener(Listener &listener)
	{
		Connection<P, MySocket>::setListener(listener);
	}

	template <class P, class MySocket>
	typename Connector<P, MySocket>::Listener *Connector<P, MySocket>::getListener() const
	{
		return dynamic_cast<Listener *>(Connection<P, MySocket>::getListener());
	}

	template <class P, class MySocket>
	void Connector<P, MySocket>::connect(
	    const String &host,
	    NetPort port,
	    Listener &listener,
	    boost::asio::io_service &ioService)
	{
		setListener(listener);
		assert(getListener());

		m_port = port;

		m_resolver.reset(new boost::asio::ip::tcp::resolver(ioService));

		boost::asio::ip::tcp::resolver::query query(
		    boost::asio::ip::tcp::v4(),
		    host,
		    "");

		const auto this_ = std::static_pointer_cast<Connector<P, MySocket> >(this->shared_from_this());
		assert(this_);

		m_resolver->async_resolve(query,
		                          bind_weak_ptr_2(this_,
		                                  std::bind(&Connector<P, MySocket>::handleResolve,
		                                          std::placeholders::_1,
		                                          std::placeholders::_2,
		                                          std::placeholders::_3)));
	}

	template <class P, class MySocket>
	std::shared_ptr<Connector<P, MySocket> > Connector<P, MySocket>::create(
	    boost::asio::io_service &service,
	    Listener *listener)
	{
		return std::make_shared<Connector<P, MySocket> >(std::unique_ptr<MySocket>(new MySocket(service)), listener);
	}


	template <class P, class MySocket>
	void Connector<P, MySocket>::handleResolve(
	    const boost::system::error_code &error,
	    boost::asio::ip::tcp::resolver::iterator iterator)
	{
		if (error)
		{
			assert(getListener());
			getListener()->connectionEstablished(false);
			this->resetListener();
			return;
		}

		m_resolver.reset();
		beginConnect(iterator);
	}

	template <class P, class MySocket>
	void Connector<P, MySocket>::beginConnect(
	    boost::asio::ip::tcp::resolver::iterator iterator)
	{
		const typename Super::Socket::endpoint_type endpoint(
		    iterator->endpoint().address(),
		    static_cast<boost::uint16_t>(m_port)
		);

		const auto this_ = std::static_pointer_cast<Connector<P, MySocket> >(this->shared_from_this());
		assert(this_);

		this->getSocket().lowest_layer().async_connect(endpoint,
		        bind_weak_ptr_1(this_,
		                        std::bind(
		                            &Connector<P, MySocket>::handleConnect,
		                            std::placeholders::_1,
		                            std::placeholders::_2,
		                            ++iterator)));
	}

	template <class P, class MySocket>
	void Connector<P, MySocket>::handleConnect(
	    const boost::system::error_code &error,
	    boost::asio::ip::tcp::resolver::iterator iterator)
	{
		if (!error)
		{
			if (getListener()->connectionEstablished(true))
			{
				this->startReceiving();
			}
			return;
		}
		else if (error == boost::asio::error::operation_aborted)
		{
			return;
		}
		else if (iterator == boost::asio::ip::tcp::resolver::iterator())
		{
			getListener()->connectionEstablished(false);
			this->resetListener();
			return;
		}

		beginConnect(iterator);
	}
}
