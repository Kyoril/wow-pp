

#pragma once

#include "common/typedefs.h"
#include "network/receive_state.h"
#include "binary_io/reader.h"
#include "binary_io/memory_source.h"

namespace wowpp
{
	namespace net
	{
		namespace http
		{
			struct IncomingAnswer : io::Reader
			{
				enum Status
				{
					OK,

					BadRequest,
					Unauthorized,
					Forbidden,
					NotFound,

					InternalServerError,
					ServiceUnavailable,

					StatusCount_
				};

				typedef std::map<String, String> Headers;


				IncomingAnswer();
				Status getStatus() const;
				const String &getStatusText() const;
				const String &getContent() const;
				const Headers &getHeaders() const;

				static ReceiveState start(IncomingAnswer &packet, io::MemorySource &source);

			private:

				Status m_status;
				String m_statusText;
				String m_content;
				Headers m_headers;
			};
		}
	}
}
