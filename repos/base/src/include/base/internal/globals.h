/*
 * \brief  Interfaces to library-global objects
 * \author Norman Feske
 * \date   2016-04-29
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__GLOBALS_H_
#define _INCLUDE__BASE__INTERNAL__GLOBALS_H_

#include <parent/parent.h>

namespace Genode {

	class Region_map;
	class Ram_allocator;
	class Env;
	class Platform;
	class Local_session_id_space;

	extern Region_map    *env_stack_area_region_map;
	extern Ram_allocator *env_stack_area_ram_allocator;

	Platform &init_platform();

	void init_stack_area();
	void init_exception_handling(Ram_allocator &, Region_map &);
	void init_signal_transmitter(Env &);
	void init_signal_receiver(Pd_session &, Parent &);
	void init_cap_slab(Pd_session &, Parent &);
	void init_cxx_heap(Ram_allocator &, Region_map &);
	void init_cxx_guard();
	void init_ldso_phdr(Env &);
	void init_signal_thread(Env &);
	void init_root_proxy(Env &);
	void init_tracing(Env &);
	void init_log(Parent &);
	void init_rpc_cap_alloc(Parent &);
	void init_parent_resource_requests(Env &);
	void init_heartbeat_monitoring(Env &);
	void init_thread(Cpu_session &, Region_map &);
	void init_thread_start(Capability<Pd_session>);
	void init_thread_bootstrap(Cpu_session &, Thread_capability);
	void exec_static_constructors();

	void cxx_demangle(char const*, char*, size_t);
	void cxx_current_exception(char *out, size_t size);
	void cxx_free_tls(void *thread);

	Id_space<Parent::Client> &env_session_id_space();

	void prepare_init_main_thread();
	void bootstrap_component(Platform &);
	void binary_ready_hook_for_platform();
}

void genode_exit(int);

#endif /* _INCLUDE__BASE__INTERNAL__GLOBALS_H_ */
