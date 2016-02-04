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

#include "numeric_editor.h"
#include "ui_numeric_editor.h"

namespace wowpp
{
	namespace editor
	{
		NumericEditor::NumericEditor(NumericProperty &prop)
			: QDialog()
			, m_ui(new Ui::NumericEditor)
            , m_property(prop)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

#ifdef WIN32
			// Not resizable
            setFixedSize(size());
			setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
#endif
            
			// Determine the value
			auto &val = m_property.getValue();
			if (val.type() == typeid(UInt32Ref))
			{
				m_ui->propValueField->setDecimals(0);
				m_ui->propValueField->setValue(static_cast<double>(boost::get<UInt32Ref&>(val).getValue()));
			}
			else if (val.type() == typeid(FloatRef))
			{
				m_ui->propValueField->setDecimals(4);
				m_ui->propValueField->setValue(static_cast<double>(boost::get<FloatRef&>(val).getValue()));
			}
            
            updateNameFields(prop);
		}

		void NumericEditor::updateNameFields(const Property &prop)
		{
			// Update name label and title
			m_ui->propNameLabel->setText(QString("%1:").arg(prop.getName().c_str()));
			setWindowTitle(QString("%1 - Numeric Value").arg(prop.getName().c_str()));
		}

		void NumericEditor::on_buttonBox_accepted()
		{
			// Determine the value
			auto &val = m_property.getValue();
			if (val.type() == typeid(UInt32Ref))
			{
				boost::get<UInt32Ref&>(val).setValue(m_ui->propValueField->value());
			}
			else if (val.type() == typeid(FloatRef))
			{
				boost::get<FloatRef&>(val).setValue(m_ui->propValueField->value());
			}
		}
	}
}
