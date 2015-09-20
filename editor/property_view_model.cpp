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

#include "property_view_model.h"

namespace wowpp
{
	namespace editor
	{
		PropertyViewModel::PropertyViewModel(Properties &properties, QObject *parent /*= nullptr*/)
			: QAbstractTableModel(parent)
			, m_properties(properties)
		{
		}

		QVariant PropertyViewModel::headerData(int section, Qt::Orientation orientation, int role) const
		{
			if (role == Qt::DisplayRole)
			{
				switch (section)
				{
					case 0:
					{
						return QString("Name");
					}

					case 1:
					{
						return QString("Value");
					}
				}
			}

			return QVariant();
		}

		int PropertyViewModel::rowCount(const QModelIndex &parent /*= QModelIndex()*/) const
		{
			// For each property there is a row
			return m_properties.size();
		}

		int PropertyViewModel::columnCount(const QModelIndex &parent /*= QModelIndex()*/) const
		{
			// Two rows: One for the property name, one for the value
			return 2;
		}

		QVariant PropertyViewModel::data(const QModelIndex &index, int role /*= Qt::DisplayRole*/) const
		{
			const int row = index.row();
			const int col = index.column();

			if (role == Qt::DisplayRole &&
				row >= 0 &&
				row <= m_properties.size())
			{
				const auto &prop = m_properties.at(row);

				switch (col)
				{
					case 0:
					{
						return QString(prop->getName().c_str());
					}

					case 1:
					{
						return QString(prop->getDisplayString().c_str());
					}
				}
			}

			//TODO
			return QVariant();
		}

		bool PropertyViewModel::setData(const QModelIndex &index, const QVariant &value, int role /*= Qt::EditRole*/)
		{
			return false;
		}

		Qt::ItemFlags PropertyViewModel::flags(const QModelIndex &index) const
		{
			const int row = index.row();

			if (row >= 0 &&
				row <= m_properties.size())
			{
				const auto &prop = m_properties.at(row);
				
				Qt::ItemFlags flags = Qt::NoItemFlags;
				if (!prop->isReadOnly())
				{
					flags |= (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
				}

				return flags;
			}

			return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
		}
	}
}
