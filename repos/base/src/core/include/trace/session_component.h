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
#include <base/session_object.h>
#include <base/tslab.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <trace_session/trace_session.h>

/* core-local includes */
#include <trace/subject_registry.h>
#include <trace/policy_registry.h>

namespace Genode { namespace Trace { class Session_component; } }


class Genode::Trace::Session_component
:
	public Session_object<Trace::Session,
	                      Trace::Session_component>,
	public Trace::Policy_owner
{
	private:

		Constrained_ram_allocator    _ram;
		Region_map                  &_local_rm;
		Sliced_heap                  _md_alloc { _ram, _local_rm };
		Tslab<Trace::Subject, 4096>  _subjects_slab;
		Tslab<Trace::Policy, 4096>   _policies_slab;
		unsigned               const _parent_levels;
		Source_registry             &_sources;
		Policy_registry             &_policies;
		Subject_registry             _subjects;
		unsigned                     _policy_cnt { 0 };
		Attached_ram_dataspace       _argument_buffer;

	public:

		/**
		 * Constructor
		 */
		Session_component(Rpc_entrypoint  &ep,
		                  Resources const &resources,
		                  Label     const &label,
		                  Diag      const &diag,
		                  Ram_allocator   &ram,
		                  Region_map      &local_rm,
		                  size_t           arg_buffer_size,
		                  unsigned         parent_levels,
		                  Source_registry &sources,
		                  Policy_registry &policies);

		~Session_component();


		/***********************
		 ** Session interface **
		 ***********************/

		Dataspace_capability dataspace();
		size_t subjects();
		size_t subject_infos();

		Policy_id alloc_policy(size_t) override;
		Dataspace_capability policy(Policy_id) override;
		void unload_policy(Policy_id) override;
		void trace(Subject_id, Policy_id, size_t) override;
		void rule(Session_label const &, Thread_name const &, Policy_id, size_t) override;
		void pause(Subject_id) override;
		void resume(Subject_id) override;
		Subject_info subject_info(Subject_id) override;
		Dataspace_capability buffer(Subject_id) override;
		void free(Subject_id) override;
};

#endif /* _CORE__INCLUDE__TRACE__SESSION_COMPONENT_H_ */
