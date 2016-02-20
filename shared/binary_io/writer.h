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
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
//

#pragma once

#include <boost/noncopyable.hpp>
#include "sink.h"

namespace io
{
	class Writer : public boost::noncopyable
	{
	public:

		explicit Writer(ISink &sink)
			: m_sink(sink)
		{
		}

		ISink &sink()
		{
			return m_sink;
		}

		template <class T>
		void writePOD(const T &pod)
		{
			m_sink.write(
			    reinterpret_cast<const char *>(&pod),
			    sizeof(pod));
		}

		template <class T>
		void writePOD(std::size_t position, const T &pod)
		{
			m_sink.overwrite(
			    position,
			    reinterpret_cast<const char *>(&pod),
			    sizeof(pod));
		}

	private:

		ISink &m_sink;
	};


#define BINARY_IO_WRITER_OPERATOR(type) \
	inline Writer &operator << (Writer &w, type value) \
	{ \
		w.writePOD(value); \
		return w; \
	}

	BINARY_IO_WRITER_OPERATOR(signed char)
	BINARY_IO_WRITER_OPERATOR(unsigned char)
	BINARY_IO_WRITER_OPERATOR(char)
	BINARY_IO_WRITER_OPERATOR(signed short)
	BINARY_IO_WRITER_OPERATOR(unsigned short)
	BINARY_IO_WRITER_OPERATOR(signed int)
	BINARY_IO_WRITER_OPERATOR(unsigned int)
	BINARY_IO_WRITER_OPERATOR(signed long)
	BINARY_IO_WRITER_OPERATOR(unsigned long)
	BINARY_IO_WRITER_OPERATOR(signed long long)
	BINARY_IO_WRITER_OPERATOR(unsigned long long)

#undef BINARY_IO_WRITER_OPERATOR

#define BINARY_IO_WRITER_FLOAT_OPERATOR(type) \
	inline Writer &operator << (Writer &w, type value) \
	{ \
		w.writePOD(value); \
		return w; \
	}

	BINARY_IO_WRITER_FLOAT_OPERATOR(float)
	BINARY_IO_WRITER_FLOAT_OPERATOR(double)
	BINARY_IO_WRITER_FLOAT_OPERATOR(long double)

#undef BINARY_IO_WRITER_FLOAT_OPERATOR

	//not implemented, use write<T> instead
	Writer &operator << (Writer &w, bool value);


	namespace detail
	{
		template <class F, class T>
		struct WriteConverted
		{
			F source;


			WriteConverted(F source)
				: source(source)
			{
			}
		};

		template <class F, class T>
		Writer &operator << (Writer &w, const WriteConverted<F, T> &surr)
		{
			return w << static_cast<T>(surr.source);
		}
	}

	template <class T, class F>
	detail::WriteConverted<F, T> write(const F &source)
	{
		return detail::WriteConverted<F, T>(source);
	}


	namespace detail
	{
		template <class I, class E>
		struct WriteRangeWithConversion
		{
			I begin, end;


			WriteRangeWithConversion(I begin, I end)
				: begin(begin)
				, end(end)
			{
			}
		};

		template <class I, class E>
		Writer &operator << (Writer &w, const WriteRangeWithConversion<I, E> &range)
		{
			for (I i = range.begin; i != range.end; ++i)
			{
				w << write<E>(*i);
			}

			return w;
		}


		template <class I>
		struct WriteRange
		{
			I begin, end;


			WriteRange(I begin, I end)
				: begin(begin)
				, end(end)
			{
			}
		};

		template <class I>
		Writer &operator << (Writer &w, const WriteRange<I> &range)
		{
			for (I i = range.begin; i != range.end; ++i)
			{
				w << *i;
			}

			return w;
		}
	}


	template <class E, class I>
	detail::WriteRangeWithConversion<I, E> write_converted_range(I begin, I end)
	{
		return detail::WriteRangeWithConversion<I, E>(begin, end);
	}

	template <class E, class R>
	detail::WriteRangeWithConversion<typename R::const_iterator, E> write_converted_range(const R &range)
	{
		return detail::WriteRangeWithConversion<typename R::const_iterator, E>(range.begin(), range.end());
	}


	template <class I>
	detail::WriteRange<I> write_range(I begin, I end)
	{
		return detail::WriteRange<I>(begin, end);
	}

	template <class R>
	detail::WriteRange<typename R::const_iterator> write_range(const R &range)
	{
		return detail::WriteRange<typename R::const_iterator>(range.begin(), range.end());
	}


	namespace detail
	{
		template <class L, class I>
		struct WriteRangeWithLength
		{
			I begin, end;


			WriteRangeWithLength(I begin, I end)
				: begin(begin)
				, end(end)
			{
			}
		};

		template <class L, class I>
		Writer &operator << (Writer &w, const WriteRangeWithLength<L, I> &range)
		{
			const L length = static_cast<L>(range.end - range.begin);

			w << length;

			for (I i = range.begin; i != range.end; ++i)
			{
				w << *i;
			}

			return w;
		}


		template <class L, class I, class E>
		struct WriteRangeWithLengthAndConversion
		{
			I begin, end;


			WriteRangeWithLengthAndConversion(I begin, I end)
				: begin(begin)
				, end(end)
			{
			}
		};

		template <class L, class I, class E>
		Writer &operator << (Writer &w, const WriteRangeWithLengthAndConversion<L, I, E> &range)
		{
			const L length = static_cast<L>(range.end - range.begin);

			w << length;

			for (I i = range.begin; i != range.end; ++i)
			{
				w << write<E>(*i);
			}

			return w;
		}
	}


	template <class L, class I>
	detail::WriteRangeWithLength<L, I> write_dynamic_range(I begin, I end)
	{
		return detail::WriteRangeWithLength<L, I>(begin, end);
	}

	template <class L, class R>
	detail::WriteRangeWithLength<L, typename R::const_iterator> write_dynamic_range(const R &range)
	{
		return detail::WriteRangeWithLength<L, typename R::const_iterator>(range.begin(), range.end());
	}


	template <class L, class E, class I>
	detail::WriteRangeWithLengthAndConversion<L, I, E> write_converted_dynamic_range(I begin, I end)
	{
		return detail::WriteRangeWithLengthAndConversion<L, I, E>(begin, end);
	}

	template <class L, class E, class R>
	detail::WriteRangeWithLengthAndConversion<L, typename R::const_iterator, E> write_converted_dynamic_range(const R &range)
	{
		return detail::WriteRangeWithLengthAndConversion<L, typename R::const_iterator, E>(range.begin(), range.end());
	}
}
