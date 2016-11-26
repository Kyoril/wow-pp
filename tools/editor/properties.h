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
#include <QMessageBox>
#include <QVariant>
#include <google/protobuf/stubs/common.h>

namespace wowpp
{
	namespace editor
	{
		/// Base class for a property
		class Property
		{
		public:

			typedef std::function<QVariant()> MiscValueCallback;

		public:

			Property(const String &name, bool readOnly = false, 
				MiscValueCallback miscCallback = MiscValueCallback())
				: m_name(name)
				, m_readOnly(readOnly)
				, m_callback(miscCallback)
			{
			}
			virtual ~Property()
			{
			}

			virtual const String &getName() const { return m_name; }
			virtual bool isReadOnly() const { return m_readOnly; }
			virtual String getDisplayString() const = 0;
			QVariant getMiscVale() const { return (m_callback ? m_callback() : QVariant()); }

		private:

			String m_name;
			bool m_readOnly;
			MiscValueCallback m_callback;
		};

		template<typename T>
		class NumericRef final
		{
			typedef std::function<T()> GetValueFunc;
			typedef std::function<void(T)> SetValueFunc;

		public:

			NumericRef(GetValueFunc getter, SetValueFunc setter)
				: m_getter(std::move(getter))
				, m_setter(std::move(setter))
			{
			}

			T getValue() const { return m_getter(); }
			void setValue(T value) { m_setter(value); }

		private:

			GetValueFunc m_getter;
			SetValueFunc m_setter;
		};

		typedef NumericRef<google::protobuf::uint32> UInt32Ref;
		typedef NumericRef<float> FloatRef;

		typedef boost::variant<UInt32Ref, FloatRef> NumericValue;

		/// This property class holds two numeric values: One minimum and one maximum value
		class MinMaxProperty : public Property
		{
		public:

			MinMaxProperty(
				const String &name, 
				const NumericValue &minValue, 
				const NumericValue &maxValue, 
				bool readOnly = false, 
				Property::MiscValueCallback miscCallback = Property::MiscValueCallback()
				)
				: Property(name, readOnly, miscCallback)
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

					auto refValue = ref.getValue();
					strm << refValue;

					auto maxRefValue = maxRef.getValue();
					if (maxRefValue != refValue)
					{
						strm << " - " << maxRefValue;
					}
				}
				else if (m_minValue.type() == typeid(FloatRef))
				{
					const FloatRef &ref = boost::get<FloatRef>(m_minValue);
					const FloatRef &maxRef = boost::get<FloatRef>(m_maxValue);
					auto refValue = ref.getValue();
					strm << refValue;

					auto maxRefValue = maxRef.getValue();
					if (maxRefValue != refValue)
					{
						strm << " - " << maxRefValue;
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
			typedef std::function<String()> GetterFunc;
			typedef std::function<void(const String&)> SetterFunc;

		public:

			StringProperty(
				const String &name, 
				GetterFunc getter,
				SetterFunc setter,
				bool readOnly = false, 
				Property::MiscValueCallback miscCallback = Property::MiscValueCallback())
				: Property(name, readOnly, miscCallback)
				, m_getter(std::move(getter))
				, m_setter(std::move(setter))
			{
			}

			virtual String getDisplayString() const override
			{
				return getValue();
			}

			String getValue() const { return m_getter(); }
			void setValue(const String &string) { m_setter(string); }

		private:

			GetterFunc m_getter;
			SetterFunc m_setter;
		};
		
		class NumericProperty : public Property
		{
		public:

			NumericProperty(
				const String &name, 
				const NumericValue &value, 
				bool readOnly = false, 
				Property::MiscValueCallback miscCallback = Property::MiscValueCallback()
				)
				: Property(name, readOnly, miscCallback)
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
