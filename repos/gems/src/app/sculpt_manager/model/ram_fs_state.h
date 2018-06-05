/*
 * \brief  Runtime state of the RAM file system
 * \author Norman Feske
 * \date   2018-05-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__RAM_FS_STATE_H_
#define _MODEL__RAM_FS_STATE_H_

/* Genode includes */
#include <util/xml_node.h>

/* local includes */
#include "types.h"

namespace Sculpt { struct Ram_fs_state; }

struct Sculpt::Ram_fs_state : Noncopyable
{
	static Ram_quota initial_ram_quota() { return Ram_quota{1024*1024}; }
	static Cap_quota initial_cap_quota() { return Cap_quota{300}; }

	Ram_quota ram_quota = initial_ram_quota();
	Cap_quota cap_quota = initial_cap_quota();

	struct Version { unsigned value; } version { 0 };

	bool inspected = false;
};

#endif /* _MODEL__RAM_FS_STATE_H_ */
