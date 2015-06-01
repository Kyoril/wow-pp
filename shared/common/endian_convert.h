
#pragma once

#include "typedefs.h"
#include <algorithm>
#include <utility>
#include <cassert>

namespace wowpp
{

#define WOWPP_LITTLE_ENDIAN 0
#define WOWPP_BIG_ENDIAN 1

#if defined(__BYTE_ORDER)
#	if (__BYTE_ORDER == __LITTLE_ENDIAN)
#		define WOWPP_ENDIANESS	WOWPP_LITTLE_ENDIAN
#	elif (__BYTE_ORDER == __BIG_ENDIAN)
#		define WOWPP_ENDIANESS	WOWPP_BIG_ENDIAN
#	endif
#elif defined(_BYTE_ORDER)
#	if (_BYTE_ORDER == _LITTLE_ENDIAN)
#		define WOWPP_ENDIANESS	WOWPP_LITTLE_ENDIAN
#	elif (_BYTE_ORDER == _BIG_ENDIAN)
#		define WOWPP_ENDIANESS	WOWPP_BIG_ENDIAN
#	endif
#elif defined(BYTE_ORDER)
#	if (BYTE_ORDER == LITTLE_ENDIAN)
#		define WOWPP_ENDIANESS	WOWPP_LITTLE_ENDIAN
#	elif (BYTE_ORDER == BIG_ENDIAN)
#		define WOWPP_ENDIANESS	WOWPP_BIG_ENDIAN
#	endif
#endif

	namespace byte_converter
	{
		template<size_t T>
		inline void convert(char* val)
		{
			std::swap(*val, *(val + T - 1));
			convert < T - 2 >(val + 1);
		}

		template<> inline void convert<0>(char*) {}
		template<> inline void convert<1>(char*) {}

		template<typename T>
		inline void apply(T* val)
		{
			convert<sizeof(T)>((char*)(val));
		}
	}

#if (WOWPP_ENDIANESS == WOWPP_BIG_ENDIAN)
	template<typename T> inline void EndianConvert(T& val) { byte_converter::apply<T>(&val); }
	template<typename T> inline void EndianConvertReverse(T&) { }
#elif (WOWPP_ENDIANESS == WOWPP_LITTLE_ENDIAN)
	template<typename T> inline void EndianConvert(T&) { }
	template<typename T> inline void EndianConvertReverse(T& val) { byte_converter::apply<T>(&val); }
#endif
}