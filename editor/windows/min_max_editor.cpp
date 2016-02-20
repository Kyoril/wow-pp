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

#include "pch.h"
#include "min_max_editor.h"
#include "ui_min_max_editor.h"

namespace wowpp
{
	namespace editor
	{
		MinMaxEditor::MinMaxEditor(MinMaxProperty &prop)
			: QDialog()
			, m_ui(new Ui::MinMaxEditor)
            , m_property(prop)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

#ifdef WIN32
			// Not resizable
			setFixedSize(size());
            setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
#endif
            
			// Update name label and title
			m_ui->propNameLabel->setText(QString("%1:").arg(prop.getName().c_str()));
			setWindowTitle(QString("%1 - Min-Max Value").arg(prop.getName().c_str()));

			// Determine the value
			auto &minVal = m_property.getMinValue();
			auto &maxVal = m_property.getMaxValue();
			if (minVal.type() == typeid(UInt32Ref))
			{
				m_ui->propMinValueField->setDecimals(0);
				m_ui->propMinValueField->setValue(static_cast<double>(boost::get<UInt32Ref&>(minVal).getValue()));
			}
			else if (minVal.type() == typeid(FloatRef))
			{
				m_ui->propMinValueField->setDecimals(4);
				m_ui->propMinValueField->setValue(static_cast<double>(boost::get<FloatRef&>(minVal).getValue()));
			}

			if (maxVal.type() == typeid(UInt32Ref))
			{
				m_ui->propMaxValueField->setDecimals(0);
				m_ui->propMaxValueField->setValue(static_cast<double>(boost::get<UInt32Ref&>(maxVal).getValue()));
			}
			else if (maxVal.type() == typeid(FloatRef))
			{
				m_ui->propMaxValueField->setDecimals(4);
				m_ui->propMaxValueField->setValue(static_cast<double>(boost::get<FloatRef&>(maxVal).getValue()));
			}
		}

		void MinMaxEditor::on_buttonBox_accepted()
		{
			// Determine the value
			auto &minVal = m_property.getMinValue();
			auto &maxVal = m_property.getMaxValue();
			if (minVal.type() == typeid(UInt32Ref))
			{
				boost::get<UInt32Ref&>(minVal).setValue(m_ui->propMinValueField->value());
			}
			else if (minVal.type() == typeid(FloatRef))
			{
				boost::get<FloatRef&>(minVal).setValue(m_ui->propMinValueField->value());
			}

			if (maxVal.type() == typeid(UInt32Ref))
			{
				boost::get<UInt32Ref&>(maxVal).setValue(m_ui->propMaxValueField->value());
			}
			else if (maxVal.type() == typeid(FloatRef))
			{
				boost::get<FloatRef&>(maxVal).setValue(m_ui->propMaxValueField->value());
			}
		}

	}
}
