/*
 * \brief  Dummy context_area helper
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <ram_session/ram_session.h>
#include <rm_session/rm_session.h>

namespace Genode {

	Rm_session *env_context_area_rm_session() { return nullptr; }

	Ram_session *env_context_area_ram_session() { return nullptr; }
}

