#pragma once

#include "common/typedefs.h"
#include "mysql_transfer.h"

namespace wowpp
{
	namespace proto
	{
		class Project;
	}

	namespace editor
	{
		TransferTask importAreaTriggers(proto::Project& project);
	}
}