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

namespace Genode { namespace Trace { class Root; } }


class Genode::Trace::Root : public Genode::Root_component<Session_component>
{
	private:

		Ram_allocator   &_ram;
		Region_map      &_local_rm;
		Source_registry &_sources;
		Policy_registry &_policies;

	protected:

		Session_component *_create_session(const char *args) override
		{
			size_t ram_quota       = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			size_t arg_buffer_size = Arg_string::find_arg(args, "arg_buffer_size").ulong_value(0);
			unsigned parent_levels = Arg_string::find_arg(args, "parent_levels").ulong_value(0);

			if (arg_buffer_size > ram_quota)
				throw Service_denied();

			return new (md_alloc())
			       Session_component(*this->ep(),
			                         session_resources_from_args(args),
			                         session_label_from_args(args),
			                         session_diag_from_args(args),
			                         _ram, _local_rm,
			                         arg_buffer_size, parent_levels,
			                         _sources, _policies);
		}

		void _upgrade_session(Session_component *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep  entry point for managing session objects
		 */
		Root(Ram_allocator &ram, Region_map &local_rm,
		     Rpc_entrypoint &session_ep, Allocator &md_alloc,
		     Source_registry &sources, Policy_registry &policies)
		:
			Root_component<Session_component>(&session_ep, &md_alloc),
			_ram(ram), _local_rm(local_rm), _sources(sources), _policies(policies)
		{ }
};

#endif /* _CORE__INCLUDE__TRACE__ROOT_H_ */
