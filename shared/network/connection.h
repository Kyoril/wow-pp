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
#include "buffer.h"
#include "receive_state.h"
#include "common/assign_on_exit.h"
#include "binary_io/string_sink.h"
#include "binary_io/memory_source.h"
#include <boost/asio.hpp>
#include <memory>
#include <array>
#include <cassert>

namespace wowpp
{
	template<class P>
	struct IConnectionListener
	{
		typedef P Protocol;

		virtual ~IConnectionListener()
		{
		}

		virtual void connectionLost() = 0;
		virtual void connectionMalformedPacket() = 0;
		virtual void connectionPacketReceived(typename Protocol::IncomingPacket &packet) = 0;
	};


	template<class P>
	class AbstractConnection
	{
	public:

		typedef P Protocol;
		typedef IConnectionListener<P> Listener;

	public:

		virtual ~AbstractConnection()
		{
		}

		virtual void setListener(Listener &Listener_) = 0;
		virtual void resetListener() = 0;
		virtual boost::asio::ip::address getRemoteAddress() const = 0;
		virtual Buffer &getSendBuffer() = 0;
		virtual void startReceiving() = 0;
		virtual void flush() = 0;
		virtual void close() = 0;

		template<class F>
		void sendSinglePacket(F generator)
		{
			io::StringSink sink(getSendBuffer());
			typename Protocol::OutgoingPacket packet(sink);
			generator(packet);
			flush();
		}
	};


	/// 
	template<class P, class MySocket = boost::asio::ip::tcp::socket>
	class Connection
        : public AbstractConnection<P>
        , public boost::noncopyable
        , public std::enable_shared_from_this<Connection<P, MySocket> >
	{
	public:

		typedef MySocket Socket;
		typedef P Protocol;
		typedef IConnectionListener<P> Listener;

	public:

		explicit Connection(std::unique_ptr<Socket> Socket_, Listener *Listener_)
			: m_socket(std::move(Socket_))
			, m_listener(Listener_)
			, m_isParsingIncomingData(false)
			, m_isClosedOnParsing(false)
		{
		}

		virtual ~Connection()
		{
		}
		
		void setListener(Listener &Listener_) override
		{
			m_listener = &Listener_;
		}

		void resetListener() override
		{
			m_listener = nullptr;
		}

		Listener *getListener() const
		{
			return m_listener;
		}

		boost::asio::ip::address getRemoteAddress() const override
		{
			return m_socket->lowest_layer().remote_endpoint().address();
		}

		Buffer &getSendBuffer() override
		{
			return m_sendBuffer;
		}

		void startReceiving() override
		{
			boost::asio::ip::tcp::no_delay Option(true);
			m_socket->lowest_layer().set_option(Option);
			beginReceive();
		}

		void flush() override
		{
			if (m_sendBuffer.empty())
			{
				return;
			}

			if (!m_sending.empty())
			{
				return;
			}

			m_sending = m_sendBuffer;
			m_sendBuffer.clear();

			assert(m_sendBuffer.empty());
			assert(!m_sending.empty());

			beginSend();
		}

		void close() override
		{
			if (m_isParsingIncomingData)
			{
				m_isClosedOnParsing = true;
				return;
			}
			else
			{
				m_isClosedOnParsing = true;
				if (m_socket->is_open())
				{
					m_socket->close();
				}
			}
		}

		void sendBuffer(const char *data, std::size_t size)
		{
			m_sendBuffer.append(data, data + size);
		}

		void sendBuffer(const Buffer &data)
		{
			m_sendBuffer(data.data(), data.size());
		}

		MySocket &getSocket() { return *m_socket; }

		static std::shared_ptr<Connection> create(boost::asio::io_service &service, Listener *listener)
		{
            return std::make_shared<Connection<P, MySocket> >(std::unique_ptr<MySocket>(new MySocket(service)), listener);
		}

	private:

		typedef std::array<char, 4096> ReceiveBuffer;

		std::unique_ptr<Socket> m_socket;
		Listener *m_listener;
		Buffer m_sending;
		Buffer m_sendBuffer;
		Buffer m_received;
		ReceiveBuffer m_receiving;
		bool m_isParsingIncomingData;
		bool m_isClosedOnParsing;

		void beginSend()
		{
			assert(!m_sending.empty());

			boost::asio::async_write(
				*m_socket,
				boost::asio::buffer(m_sending),
				std::bind(&Connection<P, Socket>::sent, this->shared_from_this(), std::placeholders::_1));
		}

		void sent(const boost::system::error_code &error)
		{
			if (error)
			{
				disconnected();
				return;
			}
			
			m_sending.clear();
			flush();
		}

		void beginReceive()
		{
			m_socket->async_read_some(
				boost::asio::buffer(m_receiving.data(), m_receiving.size()),
				std::bind(&Connection<P, Socket>::received, this->shared_from_this(), std::placeholders::_2));
		}

		void received(std::size_t size)
		{
			assert(size <= m_receiving.size());

			if (size == 0)
			{
				disconnected();
				return;
			}

			m_received.append(
				m_receiving.begin(),
				m_receiving.begin() + size);

			m_isParsingIncomingData = true;
			AssignOnExit<bool> isParsingIncomingDataResetter(
				m_isParsingIncomingData, false);

			bool nextPacket;
			std::size_t parsedUntil = 0;
			do 
			{
				nextPacket = false;

				const size_t availableSize = (m_received.size() - parsedUntil);
				const char *const packetBegin = &m_received[0] + parsedUntil;
				const char *const streamEnd = packetBegin + availableSize;

				io::MemorySource source(packetBegin, streamEnd);

				typename Protocol::IncomingPacket packet;
				const ReceiveState state = packet.start(packet, source);

				switch (state)
				{
					case receive_state::Incomplete:
					{
						break;
					}

					case receive_state::Complete:
					{
						if (m_listener)
						{
							m_listener->connectionPacketReceived(packet);
						}

						nextPacket = true;
						parsedUntil += static_cast<std::size_t>(source.getPosition() - source.getBegin());
						break;
					}

					case receive_state::Malformed:
					{
						m_socket.reset();
						if (m_listener)
						{
							m_listener->connectionMalformedPacket();
							m_listener = nullptr;
						}
						return;
					}
				}

				if (m_isClosedOnParsing)
				{
					m_isClosedOnParsing = false;
					m_socket.reset();
					return;
				}
			} while (nextPacket);

			if (parsedUntil)
			{
				assert(parsedUntil <= m_received.size());

				m_received.erase(
					m_received.begin(),
					m_received.begin() + static_cast<std::ptrdiff_t>(parsedUntil));
			}

			beginReceive();
		}

		void disconnected()
		{
			if (m_listener)
			{
				m_listener->connectionLost();
				m_listener = nullptr;
			}
		}
	};
}
