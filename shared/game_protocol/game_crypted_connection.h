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
#include "network/buffer.h"
#include "network/receive_state.h"
#include "common/assign_on_exit.h"
#include "binary_io/string_sink.h"
#include "binary_io/memory_source.h"
#include "game_protocol/game_crypt.h"
#include <boost/asio.hpp>
#include <memory>
#include <algorithm>
#include <array>
#include <cassert>

namespace wowpp
{
	namespace game
	{
		/// 
		template<class P, class MySocket = boost::asio::ip::tcp::socket>
		class CryptedConnection
			: public AbstractConnection<P>
			, public boost::noncopyable
			, public std::enable_shared_from_this<CryptedConnection<P, MySocket> >
		{
		public:

			typedef MySocket Socket;
			typedef P Protocol;
			typedef wowpp::IConnectionListener<P> Listener;

		public:

			explicit CryptedConnection(std::unique_ptr<Socket> Socket_, Listener *Listener_)
				: m_socket(std::move(Socket_))
				, m_listener(Listener_)
				, m_isParsingIncomingData(false)
				, m_isClosedOnParsing(false)
				, m_decryptedUntil(0)
			{
			}

			virtual ~CryptedConnection()
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

			game::Crypt &getCrypt()
			{
				return m_crypt;
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

			static std::shared_ptr<CryptedConnection> create(boost::asio::io_service &service, Listener *listener)
			{
				return std::make_shared<CryptedConnection<P, MySocket> >(std::unique_ptr<MySocket>(new MySocket(service)), listener);
			}

		private:

			typedef std::array<char, 4096> ReceiveBuffer;

			std::unique_ptr<Socket> m_socket;
			Listener *m_listener;
			Buffer m_sending;
			Buffer m_sendBuffer;
			Buffer m_received;
			game::Crypt m_crypt;
			ReceiveBuffer m_receiving;
			bool m_isParsingIncomingData;
			bool m_isClosedOnParsing;
			size_t m_decryptedUntil;

			void beginSend()
			{
				assert(!m_sending.empty());

				boost::asio::async_write(
					*m_socket,
					boost::asio::buffer(m_sending),
					std::bind(&CryptedConnection<P, Socket>::sent, this->shared_from_this(), std::placeholders::_1));
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
					std::bind(&CryptedConnection<P, Socket>::received, this->shared_from_this(), std::placeholders::_2));
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

					// Check if we have received a complete header
					if (m_decryptedUntil <= parsedUntil &&
						availableSize >= game::Crypt::CryptedReceiveLength)
					{
						m_crypt.decryptReceive(reinterpret_cast<UInt8*>(&m_received[0] + parsedUntil), game::Crypt::CryptedReceiveLength);

						// This will prevent double-decryption of the header (which would produce
						// invalid packet sizes)
						m_decryptedUntil = parsedUntil + game::Crypt::CryptedReceiveLength;
					}
					
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

					// Reset
					m_decryptedUntil = 0;
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
}
