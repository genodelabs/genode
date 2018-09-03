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
#include <types.h>
#include <model/child_state.h>

namespace Sculpt { struct Ram_fs_state; }

struct Sculpt::Ram_fs_state : Child_state
{
	bool inspected = false;

	Ram_fs_state(Start_name const &name)
	: Child_state(name, Ram_quota{1024*1024}, Cap_quota{300}) { }
};

#endif /* _MODEL__RAM_FS_STATE_H_ */
