/*
 * \brief  Sculpt version
 * \author Norman Feske
 * \date   2019-02-24
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__SCULPT_VERSION_H_
#define _MODEL__SCULPT_VERSION_H_

#include <types.h>

namespace Sculpt { struct Sculpt_version; }

struct Sculpt::Sculpt_version : String<6>
{
	Sculpt_version(Env &env)
	: String<6>(Attached_rom_dataspace(env, "VERSION").local_addr<char const>())
	{ }
};

#endif /* _MODEL__SCULPT_VERSION_H_ */
