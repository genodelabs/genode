/*
 * \brief  Implementation of the CPU session interface
 * \author Christian Prochaska
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include "cpu_session_component.h"
#include <util/arg_string.h>
#include <util/list.h>

using namespace Genode;
using namespace Cpu_sampler;


Thread_capability
Cpu_sampler::Cpu_session_component::create_thread(Pd_session_capability  pd,
                                                  Name            const &name,
                                                  Affinity::Location     affinity,
                                                  Weight                 weight,
                                                  addr_t                 utcb)
{
	Cpu_thread_component *cpu_thread = new (_md_alloc)
		Cpu_thread_component(*this,
	                         _md_alloc,
	                          pd,
	                          name,
	                          affinity,
	                          weight,
	                          utcb,
	                          name.string(),
	                          _next_thread_id);

	_thread_list.insert(new (_md_alloc) Thread_element(cpu_thread));

	_thread_list_change_handler.thread_list_changed();

	_next_thread_id++;

	return cpu_thread->cap();
}


void Cpu_sampler::Cpu_session_component::kill_thread(Thread_capability thread_cap)
{
	auto lambda = [&] (Thread_element *cpu_thread_element) {

		Cpu_thread_component *cpu_thread = cpu_thread_element->object();

		if (cpu_thread->cap() == thread_cap) {
			_thread_list.remove(cpu_thread_element);
			destroy(_md_alloc, cpu_thread_element);
			destroy(_md_alloc, cpu_thread);
			_thread_list_change_handler.thread_list_changed();
		}
	};

	for_each_thread(_thread_list, lambda);

	_parent_cpu_session.kill_thread(thread_cap);
}


void
Cpu_sampler::Cpu_session_component::exception_sigh(Signal_context_capability handler)
{
	_parent_cpu_session.exception_sigh(handler);
}


Affinity::Space Cpu_sampler::Cpu_session_component::affinity_space() const
{
	return _parent_cpu_session.affinity_space();
}


Dataspace_capability
Cpu_sampler::Cpu_session_component::trace_control()
{
	return _parent_cpu_session.trace_control();
}


Cpu_sampler::
Cpu_session_component::
Cpu_session_component(Rpc_entrypoint             &thread_ep,
                      Env                        &env,
                      Allocator                  &md_alloc,
                      Thread_list                &thread_list,
                      Thread_list_change_handler &thread_list_change_handler,
                      char                 const *args)
: _thread_ep(thread_ep),
  _env(env),
  _id_space_element(_parent_client, _env.id_space()),
  _parent_cpu_session(_env.session<Cpu_session>(_id_space_element.id(), args, Affinity())),
  _md_alloc(md_alloc),
  _thread_list(thread_list),
  _thread_list_change_handler(thread_list_change_handler),
  _session_label(label_from_args(args)),
  _native_cpu_cap(_setup_native_cpu())
{ }


void Cpu_sampler::Cpu_session_component::upgrade_ram_quota(size_t ram_quota)
{
	String<64> const args("ram_quota=", ram_quota);
	_env.upgrade(_id_space_element.id(), args.string());
}


Cpu_sampler::Cpu_session_component::~Cpu_session_component()
{
	_cleanup_native_cpu();

	auto lambda = [&] (Thread_element *cpu_thread_element) {

		Cpu_thread_component *cpu_thread = cpu_thread_element->object();

		if (cpu_thread->cpu_session_component() == this) {
			_thread_list.remove(cpu_thread_element);
			destroy(_md_alloc, cpu_thread_element);
			destroy(_md_alloc, cpu_thread);
		}
	};

	for_each_thread(_thread_list, lambda);

	_thread_list_change_handler.thread_list_changed();
}


int Cpu_sampler::Cpu_session_component::ref_account(Cpu_session_capability cap)
{
	return _parent_cpu_session.ref_account(cap);
}


int Cpu_sampler::Cpu_session_component::transfer_quota(Cpu_session_capability cap,
                                                       size_t size)
{
	return _parent_cpu_session.transfer_quota(cap, size);
}


Cpu_session::Quota Cpu_sampler::Cpu_session_component::quota()
{
	return _parent_cpu_session.quota();
}


Capability<Cpu_session::Native_cpu>
Cpu_sampler::Cpu_session_component::native_cpu()
{
	return _native_cpu_cap;
}
