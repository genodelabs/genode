/*
 * \brief  Log root interface
 * \author Norman Feske
 * \date   2006-09-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__LOG_ROOT_H_
#define _CORE__INCLUDE__LOG_ROOT_H_

#include <root/component.h>
#include <util/arg_string.h>

/* core includes */
#include <log_session_component.h>

namespace Core { struct Log_root; }


struct Core::Log_root : Root_component<Log_session_component>
{
	Create_result _create_session(const char *args) override
	{
		return _alloc_obj(label_from_args(args));
	}

	Log_root(Rpc_entrypoint &session_ep, Allocator &md_alloc)
	:
		Root_component<Log_session_component>(&session_ep, &md_alloc)
	{ }
};

#endif /* _CORE__INCLUDE__LOG_ROOT_H_ */
