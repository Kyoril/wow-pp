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

#include "constants.h"
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace wowpp
{
	static std::default_random_engine randomGenerator;

	template <class T>
	T limit(T value, T min, T max)
	{
		return std::min(max, std::max(value, min));
	}

	static inline std::string &ltrim(std::string &s)
	{
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
		return s;
	}

	static inline std::string &rtrim(std::string &s)
	{
		s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
		return s;
	}

	static inline std::string &trim(std::string &s)
	{
		return ltrim(rtrim(s));
	}

	static inline std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &out_elems)
	{
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim))
		{
			if (!item.empty())
			{
				out_elems.emplace_back(item);
			}
		}

		return out_elems;
	}

	static inline void capitalize(std::string& word)
	{
		std::transform(word.begin(), word.end(), word.begin(), ::tolower);
		word[0] = std::toupper(word[0]);
	}

	template<class T>
	T interpolate(T min, T max, float t)
	{
		// Some optimization
		if (t <= 0.0f)
		{
			return min;
		}
		else if (t >= 1.0f)
		{
			return max;
		}

		// Linear interpolation
		return min + (max - min) * t;
	}
}
