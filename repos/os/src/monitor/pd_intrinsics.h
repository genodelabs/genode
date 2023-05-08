/*
 * \brief  Sandbox::Pd_intrinsics for intercepting the PD access of children
 * \author Norman Feske
 * \date   2023-05-10
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PD_INTRINSICS_H_
#define _PD_INTRINSICS_H_

/* Genode includes */
#include <pd_session/connection.h>

/* local includes */
#include <monitored_cpu.h>

namespace Monitor { struct Pd_intrinsics; }


struct Monitor::Pd_intrinsics : Sandbox::Pd_intrinsics
{
	Env &_env;

	/*
	 * The sandbox interacts with the 'Monitored_ref_pd' only for quota
	 * transfers to the children, in particular during the child creation.
	 */
	struct Monitored_ref_pd : Monitored_pd_session
	{
		using Monitored_pd_session::Monitored_pd_session;

		using Sig_src_cap   = Signal_source_capability;
		using Sig_ctx_cap   = Signal_context_capability;
		using Ram_ds_cap    = Ram_dataspace_capability;
		using Mng_sys_state = Managing_system_state;

		void                   assign_parent(Capability<Parent>)         override { never_called(__func__); };
		bool                   assign_pci(addr_t, uint16_t)              override { never_called(__func__); };
		void                   map(addr_t, addr_t)                       override { never_called(__func__); };
		Sig_src_cap            alloc_signal_source()                     override { never_called(__func__); };
		void                   free_signal_source(Sig_src_cap)           override { never_called(__func__); };
		Sig_ctx_cap            alloc_context(Sig_src_cap, unsigned long) override { never_called(__func__); };
		void                   free_context(Sig_ctx_cap)                 override { never_called(__func__); };
		void                   submit(Sig_ctx_cap, unsigned)             override { never_called(__func__); };
		Native_capability      alloc_rpc_cap(Native_capability)          override { never_called(__func__); };
		void                   free_rpc_cap(Native_capability)           override { never_called(__func__); };
		Capability<Region_map> address_space()                           override { never_called(__func__); };
		Capability<Region_map> stack_area()                              override { never_called(__func__); };
		Capability<Region_map> linker_area()                             override { never_called(__func__); };
		Cap_quota              cap_quota() const                         override { never_called(__func__); };
		Cap_quota              used_caps() const                         override { never_called(__func__); };
		Alloc_result           try_alloc(size_t, Cache)                  override { never_called(__func__); };
		void                   free(Ram_ds_cap)                          override { never_called(__func__); };
		size_t                 dataspace_size(Ram_ds_cap) const          override { never_called(__func__); };
		Ram_quota              ram_quota() const                         override { never_called(__func__); };
		Ram_quota              used_ram()  const                         override { never_called(__func__); };
		Capability<Native_pd>  native_pd()                               override { never_called(__func__); };
		Mng_sys_state          managing_system(Mng_sys_state const &)    override { never_called(__func__); };
		addr_t                 dma_addr(Ram_ds_cap)                      override { never_called(__func__); };
		Attach_dma_result      attach_dma(Dataspace_capability, addr_t)  override { never_called(__func__); };

	} _monitored_ref_pd { _env.ep(), _env.pd_session_cap(), Session::Label { } };

	struct Monitored_ref_cpu : Monitored_cpu_session
	{
		using Monitored_cpu_session::Monitored_cpu_session;

		using Sig_ctx_cap   = Signal_context_capability;

		Thread_capability
		create_thread(Capability<Pd_session>, Cpu_session::Name const &,
		              Affinity::Location, Weight, addr_t)     override { never_called(__func__); }
		void                   kill_thread(Thread_capability) override { never_called(__func__); }
		void                   exception_sigh(Sig_ctx_cap)    override { never_called(__func__); }
		Affinity::Space        affinity_space() const         override { never_called(__func__); }
		Dataspace_capability   trace_control()                override { never_called(__func__); }
		Quota                  quota()                        override { never_called(__func__); }
		Capability<Native_cpu> native_cpu()                   override { never_called(__func__); }

	} _monitored_ref_cpu { _env.ep(), _env.cpu_session_cap(), Session::Label { } };

	void with_intrinsics(Capability<Pd_session> pd_cap, Pd_session &pd, Fn const &fn) override
	{
		/*
		 * Depending on the presence of the PD session in our local entrypoint,
		 * we know whether the child is to be monitored or not.
		 *
		 * If not monitored, we provide the default PD intrinsics as done by
		 * init, using the 'Env' as reference PD session and accessing the
		 * child's address space via RPC. For such childen, the monitor
		 * functions exactly like init with no indirection.
		 *
		 * For monitored childen, we provide the '_monitored_ref_pd' and
		 * '_monitored_ref_cpu' to get hold of all interactions between the
		 * child and its reference PD. Furthermore, the sandbox' interplay with
		 * the child's address space is redirected to the locally implemented
		 * 'Monitored_region_map'.
		 */
		Inferior_pd::with_inferior_pd(_env.ep(), pd_cap,
			[&] (Inferior_pd &inferior_pd) {

				Intrinsics intrinsics { .ref_pd        = _monitored_ref_pd,
				                        .ref_pd_cap    = _monitored_ref_pd.cap(),
				                        .ref_cpu       = _monitored_ref_cpu,
				                        .ref_cpu_cap   = _monitored_ref_cpu.cap(),
				                        .address_space = inferior_pd._address_space };
				fn.call(intrinsics);
			},
			[&] /* PD session not intercepted */ {

				Region_map_client region_map(pd.address_space());

				Intrinsics intrinsics { .ref_pd        = _env.pd(),
				                        .ref_pd_cap    = _env.pd_session_cap(),
				                        .ref_cpu       = _env.cpu(),
				                        .ref_cpu_cap   = _env.cpu_session_cap(),
				                        .address_space = region_map };
				fn.call(intrinsics);
			}
		);
	}

	void start_initial_thread(Capability<Cpu_thread> cap, addr_t ip) override
	{
		Monitored_thread::with_thread(_env.ep(), cap,
			[&] (Monitored_thread &monitored_thread) {
				monitored_thread.start(ip, 0);
			},
			[&] /* PD session not intercepted */ {
				Cpu_thread_client(cap).start(ip, 0);
			});
	}

	Pd_intrinsics(Env &env) : _env(env) { }
};

#endif /* _PD_INTRINSICS_H_ */
