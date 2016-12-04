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
#include "variable_view_model.h"
#include "proto_data/project.h"

namespace wowpp
{
	namespace editor
	{
		VariableViewModel::VariableViewModel(proto::Project &project, QObject *parent /*= nullptr*/)
			: QAbstractTableModel(parent)
			, m_project(project)
		{
		}

		QVariant VariableViewModel::headerData(int section, Qt::Orientation orientation, int role) const
		{
			if (role == Qt::DisplayRole)
			{
				switch (section)
				{
					case 0:
					{
						return QString("ID");
					}

					case 1:
					{
						return QString("Name");
					}

					case 2:
					{
						return QString("Type");
					}

					case 3:
					{
						return QString("Default Value");
					}
				}
			}

			return QVariant();
		}

		int VariableViewModel::rowCount(const QModelIndex &parent /*= QModelIndex()*/) const
		{
			// For each property there is a row
			return m_project.variables.getTemplates().entry_size();
		}

		int VariableViewModel::columnCount(const QModelIndex &parent /*= QModelIndex()*/) const
		{
			return 4;
		}

		QVariant VariableViewModel::data(const QModelIndex &index, int role /*= Qt::DisplayRole*/) const
		{
			const int row = index.row();
			const int col = index.column();

			if (role == Qt::DisplayRole &&
				row >= 0 &&
				row <= m_project.variables.getTemplates().entry_size())
			{
				const auto &variable = m_project.variables.getTemplates().entry(row);
				switch (col)
				{
					case 0:
					{
						return QString("%0%").arg(variable.id());
					}

					case 1:
					{
						return QString(variable.name().c_str());
					}

					case 2:
					{
						switch (variable.data_case())
						{
							case proto::VariableEntry::kFloatvalue:
								return QString("FLOAT");
							case proto::VariableEntry::kIntvalue:
								return QString("INTEGER");
							case proto::VariableEntry::kLongvalue:
								return QString("LONG");
							case proto::VariableEntry::kStringvalue:
								return QString("STRING");
							default:
								return QString("UNKNOWN");
						}
					}

					case 3:
					{
						switch (variable.data_case())
						{
							case proto::VariableEntry::kFloatvalue:
								return QString("%0%").arg(variable.floatvalue());
							case proto::VariableEntry::kIntvalue:
								return QString("%0%").arg(variable.intvalue());
							case proto::VariableEntry::kLongvalue:
								return QString("%0%").arg(variable.longvalue());
							case proto::VariableEntry::kStringvalue:
								return QString(variable.stringvalue().c_str());
						}
					}
				}
			}

			return QVariant();
		}

		bool VariableViewModel::setData(const QModelIndex &index, const QVariant &value, int role /*= Qt::EditRole*/)
		{
			return false;
		}

		Qt::ItemFlags VariableViewModel::flags(const QModelIndex &index) const
		{
			const int row = index.row();

			/*if (row >= 0 &&
				row <= m_properties.size())
			{
				const auto &prop = m_properties.at(row);
				
				Qt::ItemFlags flags = Qt::NoItemFlags;
				if (!prop->isReadOnly())
				{
					flags |= (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
				}

				return flags;
			}*/

			switch (index.column())
			{
				case 0:
					return Qt::ItemIsSelectable;
					break;
				default:
					return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
			}
			if (index.column() == 0)
			{
				return Qt::ItemIsSelectable;
			}

			return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
		}
	}
}
