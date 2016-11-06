/*
 * \brief  CPU session component interface
 * \author Christian Prochaska
 * \date   2016-01-18
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_SESSION_COMPONENT_H_
#define _CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <cpu_session/client.h>
#include <os/session_policy.h>

/* local includes */
#include "cpu_thread_component.h"
#include "thread_list_change_handler.h"

namespace Cpu_sampler {
	using namespace Genode;
	class Cpu_session_component;
	typedef List<List_element<Cpu_thread_component>> Thread_list;
	typedef List_element<Cpu_thread_component>       Thread_element;

	template <typename FN>
	void for_each_thread(Thread_list &thread_list, FN const &fn);
}


template <typename FN>
void Cpu_sampler::for_each_thread(Thread_list &thread_list, FN const &fn)
{
	Thread_element *next_cpu_thread_element = 0;

	for (Thread_element *cpu_thread_element = thread_list.first();
		 cpu_thread_element;
		 cpu_thread_element = next_cpu_thread_element) {

		 next_cpu_thread_element = cpu_thread_element->next();

		 fn(cpu_thread_element);
	}
}


class Cpu_sampler::Cpu_session_component : public Rpc_object<Cpu_session>
{
	private:

		Rpc_entrypoint                          &_thread_ep;
		Env                                     &_env;
		Parent::Client                           _parent_client;
		Id_space<Parent::Client>::Element const  _id_space_element;
		Cpu_session_client                       _parent_cpu_session;
		Allocator                               &_md_alloc;
		Thread_list                             &_thread_list;
		Thread_list_change_handler              &_thread_list_change_handler;
		Session_label                            _session_label;
		unsigned int                             _next_thread_id = 0;
		Capability<Cpu_session::Native_cpu>      _native_cpu_cap;
		Capability<Cpu_session::Native_cpu>      _setup_native_cpu();
		void _cleanup_native_cpu();

	public:

		Session_label &session_label() { return _session_label; }
		Cpu_session_client &parent_cpu_session() { return _parent_cpu_session; }
		Rpc_entrypoint &thread_ep() { return _thread_ep; }

		/**
		 * Constructor
		 */
		Cpu_session_component(Rpc_entrypoint             &thread_ep,
		                      Env                        &env,
		                      Allocator                  &md_alloc,
		                      Thread_list                &thread_list,
		                      Thread_list_change_handler &thread_list_change_handler,
		                      char                 const *args);

		/**
		 * Destructor
		 */
		~Cpu_session_component();

		void upgrade_ram_quota(size_t ram_quota);


		/***************************
		 ** CPU session interface **
		 ***************************/

		Thread_capability create_thread(Pd_session_capability pd,
		                                Name const &,
		                                Affinity::Location,
		                                Weight,
		                                addr_t) override;
		void kill_thread(Thread_capability) override;
		void exception_sigh(Signal_context_capability handler) override;
		Affinity::Space affinity_space() const override;
		Dataspace_capability trace_control() override;
		int ref_account(Cpu_session_capability c) override;
		int transfer_quota(Cpu_session_capability c, size_t q) override;
		Quota quota() override;
		Capability<Cpu_session::Native_cpu> native_cpu() override;
};

#endif /* _CPU_SESSION_COMPONENT_H_ */
