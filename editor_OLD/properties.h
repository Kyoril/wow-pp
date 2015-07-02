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
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#pragma once

#include "common/typedefs.h"
#include <boost/variant.hpp>
#include <QMessageBox>
#include "log/default_log_levels.h"
#include <sstream>
#include <memory>
#include <vector>

namespace wowpp
{
	namespace editor
	{
		/// Base class for a property
		class Property
		{
		public:

			Property(const String &name, bool readOnly = false)
				: m_name(name)
				, m_readOnly(readOnly)
			{
			}
			virtual ~Property()
			{
			}

			virtual const String &getName() const { return m_name; }
			virtual bool isReadOnly() const { return m_readOnly; }
			virtual String getDisplayString() const = 0;

		private:

			String m_name;
			bool m_readOnly;
		};

		template<typename T>
		class NumericRef final
		{
		public:

			NumericRef(T &value)
				: m_value(value)
			{
			}

			T &getValue() { return m_value; }
			const T &getValue() const { return m_value; }

		private:

			T &m_value;
		};

		typedef NumericRef<UInt32> UInt32Ref;
		typedef NumericRef<float> FloatRef;

		typedef boost::variant<UInt32Ref, FloatRef> NumericValue;

		/// This property class holds two numeric values: One minimum and one maximum value
		class MinMaxProperty : public Property
		{
		public:

			MinMaxProperty(const String &name, const NumericValue &minValue, const NumericValue &maxValue, bool readOnly = false)
				: Property(name, readOnly)
				, m_minValue(minValue)
				, m_maxValue(maxValue)
			{
			}

			virtual String getDisplayString() const override
			{
				std::ostringstream strm;
				if (m_minValue.type() == typeid(UInt32Ref))
				{
					const UInt32Ref &ref = boost::get<UInt32Ref>(m_minValue);
					const UInt32Ref &maxRef = boost::get<UInt32Ref>(m_maxValue);
					strm << ref.getValue();

					if (maxRef.getValue() != ref.getValue())
					{
						strm << " - " << maxRef.getValue();
					}
				}
				else if (m_minValue.type() == typeid(FloatRef))
				{
					const FloatRef &ref = boost::get<FloatRef>(m_minValue);
					const FloatRef &maxRef = boost::get<FloatRef>(m_maxValue);
					strm << ref.getValue();

					if (maxRef.getValue() != ref.getValue())
					{
						strm << " - " << maxRef.getValue();
					}
				}

				return strm.str();
			}

			NumericValue &getMinValue() { return m_minValue; }
			NumericValue &getMaxValue() { return m_maxValue; }

		private:

			NumericValue m_minValue;
			NumericValue m_maxValue;
		};
		
		class StringProperty : public Property
		{
		public:

			StringProperty(const String &name, String &value, bool readOnly = false)
				: Property(name, readOnly)
				, m_value(value)
			{
			}

			virtual String getDisplayString() const override
			{
				return m_value;
			}

			String &getValue() { return m_value; }

		private:

			String &m_value;
		};
		
		class NumericProperty : public Property
		{
		public:

			NumericProperty(const String &name, const NumericValue &value, bool readOnly = false)
				: Property(name, readOnly)
				, m_value(value)
			{
			}

			virtual String getDisplayString() const override
			{
				std::ostringstream strm;

				if (m_value.type() == typeid(UInt32Ref))
				{
					const UInt32Ref &ref = boost::get<UInt32Ref>(m_value);
					strm << ref.getValue();
				}
				else if (m_value.type() == typeid(FloatRef))
				{
					const FloatRef &ref = boost::get<FloatRef>(m_value);
					strm << ref.getValue();
				}
				return strm.str();
			}

			NumericValue &getValue() { return m_value; }

		private:

			NumericValue m_value;
		};

		typedef std::unique_ptr<Property> PropertyPtr;
		typedef std::vector<PropertyPtr> Properties;
	}
}
