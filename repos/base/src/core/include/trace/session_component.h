/*
 * \brief  TRACE session implementation
 * \author Norman Feske
 * \date   2013-08-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRACE__SESSION_COMPONENT_H_
#define _CORE__INCLUDE__TRACE__SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator_guard.h>
#include <base/rpc_server.h>
#include <base/tslab.h>
#include <trace_session/trace_session.h>

/* core-local includes */
#include <trace/subject_registry.h>
#include <trace/policy_registry.h>

namespace Genode { namespace Trace { class Session_component; } }


class Genode::Trace::Session_component
:
	public Genode::Rpc_object<Genode::Trace::Session,
	                          Genode::Trace::Session_component>,
	public Genode::Trace::Policy_owner
{
	private:

		Ram_session                 &_ram;
		Allocator_guard              _md_alloc;
		Tslab<Trace::Subject, 4096>  _subjects_slab;
		Tslab<Trace::Policy, 4096>   _policies_slab;
		unsigned                     _parent_levels;
		Session_label                _label;
		Source_registry             &_sources;
		Policy_registry             &_policies;
		Subject_registry             _subjects;
		unsigned                     _policy_cnt;

		struct Argument_buffer
		{
			Ram_session             &ram;
			Ram_dataspace_capability ds;
			char                    *base;
			size_t                   size;

			Argument_buffer(Ram_session &ram, size_t size)
			:
				ram(ram),
				ds(ram.alloc(size)),
				base(env_deprecated()->rm_session()->attach(ds)),
				size(ram.dataspace_size(ds))
			{ }

			~Argument_buffer()
			{
				env_deprecated()->rm_session()->detach(base);
				ram.free(ds);
			}
		} _argument_buffer;

	public:

		/**
		 * Constructor
		 */
		Session_component(Allocator &md_alloc, size_t ram_quota,
		                 size_t arg_buffer_size, unsigned parent_levels,
		                 char const *label, Source_registry &sources,
		                 Policy_registry &policies);

		~Session_component();

		/**
		 * Register quota donation at allocator guard
		 */
		void upgrade_ram_quota(size_t ram_quota) { _md_alloc.upgrade(ram_quota); }


		/***********************
		 ** Session interface **
		 ***********************/

		Dataspace_capability dataspace();
		size_t subjects();

		Policy_id alloc_policy(size_t);
		Dataspace_capability policy(Policy_id);
		void unload_policy(Policy_id);
		void trace(Subject_id, Policy_id, size_t);
		void rule(Session_label const &, Thread_name const &, Policy_id, size_t);
		void pause(Subject_id);
		void resume(Subject_id);
		Subject_info subject_info(Subject_id);
		Dataspace_capability buffer(Subject_id);
		void free(Subject_id);
};

#endif /* _CORE__INCLUDE__TRACE__SESSION_COMPONENT_H_ */
