
#pragma once

namespace wowpp
{
	class GameUnit;

	/// This class handles spell casting logic for an executing unit object.
	class SpellCast final
	{
	public:

		explicit SpellCast(GameUnit &executer);

	private:

		GameUnit &m_executer;
	};
}
