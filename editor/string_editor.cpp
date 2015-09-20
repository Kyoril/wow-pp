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

#include "string_editor.h"
#include "ui_string_editor.h"

namespace wowpp
{
	namespace editor
	{
		StringEditor::StringEditor(StringProperty &prop)
			: QDialog()
			, m_ui(new Ui::StringEditor)
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
			setWindowTitle(QString("%1 - String Value").arg(prop.getName().c_str()));

			m_ui->propValueField->setText(prop.getDisplayString().c_str());
		}

		void StringEditor::on_buttonBox_accepted()
		{
			auto &str = m_property.getValue();
			str = m_ui->propValueField->text().toStdString();
		}

	}
}
