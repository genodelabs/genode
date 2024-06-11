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

/* core includes */
#include <trace/subject_registry.h>
#include <trace/policy_registry.h>

namespace Core { namespace Trace { class Session_component; } }


class Core::Trace::Session_component
:
	public Session_object<Trace::Session, Trace::Session_component>,
	public Trace::Policy_owner
{
	private:

		Constrained_ram_allocator    _ram;
		Region_map                  &_local_rm;
		Sliced_heap                  _md_alloc { _ram, _local_rm };
		Tslab<Trace::Subject, 4096>  _subjects_slab;
		Tslab<Trace::Policy, 4096>   _policies_slab;
		Source_registry             &_sources;
		Policy_registry             &_policies;
		Subject_registry             _subjects;
		unsigned                     _policy_cnt { 0 };
		Attached_ram_dataspace       _argument_buffer;

		/*
		 * Whenever a trace session is deliberately labeled as empty by the
		 * top-level init instance, the session is granted global reach.
		 * Otherwise, the label is taken a prefix filter for the visibility
		 * of trace subjects within the session.
		 */
		Filter _filter() const
		{
			return (_label == "init -> ") ? Filter("") : Filter(_label);
		}

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
		                  Source_registry &sources,
		                  Policy_registry &policies);

		~Session_component();


		/***********************
		 ** Session interface **
		 ***********************/

		Dataspace_capability dataspace();
		Subjects_rpc_result subjects();
		Infos_rpc_result subject_infos();
		Alloc_policy_rpc_result alloc_policy(Policy_size);
		Dataspace_capability policy(Policy_id);
		void unload_policy(Policy_id);
		Trace_rpc_result trace(Subject_id, Policy_id, Buffer_size);
		void pause(Subject_id);
		void resume(Subject_id);
		Dataspace_capability buffer(Subject_id);
		void free(Subject_id);
};

#endif /* _CORE__INCLUDE__TRACE__SESSION_COMPONENT_H_ */
