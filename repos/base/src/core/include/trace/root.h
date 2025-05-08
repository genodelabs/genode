/*
 * \brief  TRACE root interface
 * \author Norman Feske
 * \date   2013-08-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRACE__ROOT_H_
#define _CORE__INCLUDE__TRACE__ROOT_H_

/* Genode includes */
#include <root/component.h>

/* core-internal includes */
#include <trace/session_component.h>

namespace Core { namespace Trace { struct Root; } }


struct Core::Trace::Root : Root_component<Session_component>
{
	Ram_allocator   &_ram;
	Local_rm        &_local_rm;
	Source_registry &_sources;
	Policy_registry &_policies;

	Create_result _create_session(const char *args) override
	{
		size_t ram_quota       = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
		size_t arg_buffer_size = Arg_string::find_arg(args, "arg_buffer_size").ulong_value(0);

		if (arg_buffer_size > ram_quota)
			return Create_error::INSUFFICIENT_RAM;

		return _alloc_obj(*this->ep(),
		                  session_resources_from_args(args),
		                  session_label_from_args(args),
		                  session_diag_from_args(args),
		                  _ram, _local_rm,
		                  arg_buffer_size,
		                  _sources, _policies);
	}

	void _upgrade_session(Session_component &s, const char *args) override
	{
		s.upgrade(ram_quota_from_args(args));
		s.upgrade(cap_quota_from_args(args));
	}

	/**
	 * Constructor
	 *
	 * \param session_ep  entry point for managing session objects
	 */
	Root(Ram_allocator &ram, Local_rm &local_rm,
	     Rpc_entrypoint &session_ep, Allocator &md_alloc,
	     Source_registry &sources, Policy_registry &policies)
	:
		Root_component<Session_component>(&session_ep, &md_alloc),
		_ram(ram), _local_rm(local_rm), _sources(sources), _policies(policies)
	{ }
};

#endif /* _CORE__INCLUDE__TRACE__ROOT_H_ */
