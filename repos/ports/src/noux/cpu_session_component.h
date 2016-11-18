/*
 * \brief  CPU session provided to Noux processes
 * \author Norman Feske
 * \date   2012-02-22
 *
 * The custom implementation of the CPU session interface is used to tweak
 * the startup procedure as performed by the 'Process' class. Normally,
 * processes start execution immediately at creation time at the ELF entry
 * point. For implementing fork semantics, however, this default behavior
 * does not work. Instead, we need to defer the start of the main thread
 * until we have finished copying the address space of the forking process.
 * Furthermore, we need to start the main thread at a custom trampoline
 * function rather than at the ELF entry point. Those customizations are
 * possible by wrapping core's CPU service.
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__CPU_SESSION_COMPONENT_H_
#define _NOUX__CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/child.h>
#include <base/rpc_server.h>
#include <cpu_session/connection.h>
#include <cpu_thread/client.h>

/* Noux includes */
#include <pd_session_component.h>

namespace Noux {

	using namespace Genode;

	class Cpu_session_component : public Rpc_object<Cpu_session>
	{
		private:

			Rpc_entrypoint &_ep;
			bool const      _forked;
			Cpu_connection  _cpu;

			enum { MAX_THREADS = 8, MAIN_THREAD_IDX = 0 };

			Thread_capability     _threads[MAX_THREADS];
			Dataspace_capability  _trace_control;
			Dataspace_registry   &_registry;

		public:

			/**
			 * Constructor
			 *
			 * \param forked   false if the CPU session belongs to a child
			 *                 created via execve or to the init process, or
			 *                 true if the CPU session belongs to a newly
			 *                 forked process.
			 *
			 * The 'forked' parameter controls the policy applied to the
			 * startup of the main thread.
			 */
			Cpu_session_component(Rpc_entrypoint &ep, Child_policy::Name const &label,
			                      bool forked, Dataspace_registry &registry)
			: _ep(ep), _forked(forked), _cpu(label.string()), _registry(registry)
			{ _ep.manage(this); }

			~Cpu_session_component()
			{
				 _ep.dissolve(this);

				if (!_trace_control.valid())
					return;

				auto lambda = [&] (Static_dataspace_info *rdi) { return rdi; };
				Static_dataspace_info *ds_info = _registry.apply(_trace_control, lambda);
				if (ds_info)
					destroy(env()->heap(), ds_info);
			}

			/**
			 * Explicitly start main thread, only meaningful when
			 * 'forked' is true
			 */
			void start_main_thread(addr_t ip, addr_t sp)
			{
				Capability<Cpu_thread> main_thread = _threads[MAIN_THREAD_IDX];
				Cpu_thread_client(main_thread).start(ip, sp);
			}

			Cpu_session_capability cpu_cap() { return _cpu.cap(); }


			/***************************
			 ** Cpu_session interface **
			 ***************************/

			Thread_capability create_thread(Capability<Pd_session> pd_cap,
			                                Name const &name,
			                                Affinity::Location affinity,
			                                Weight weight,
			                                addr_t utcb) override
			{
				/* create thread at core, keep local copy (needed on NOVA) */
				for (unsigned i = 0; i < MAX_THREADS; i++) {
					if (_threads[i].valid())
						continue;

					auto lambda = [&] (Pd_session_component *pd)
					{
						if (!pd)
							throw Thread_creation_failed();

						return _cpu.create_thread(pd->core_pd_cap(), name,
						                          affinity, weight, utcb);
					};

					Thread_capability cap = _ep.apply(pd_cap, lambda);
					_threads[i] = cap;
					return cap;
				}

				error("maximum number of threads per session reached");
				throw Thread_creation_failed();
			}

			void kill_thread(Thread_capability thread) override
			{
				/* purge local copy of thread capability */
				for (unsigned i = 0; i < MAX_THREADS; i++)
					if (_threads[i].local_name() == thread.local_name())
						_threads[i] = Thread_capability();

				_cpu.kill_thread(thread);
			}

			void exception_sigh(Signal_context_capability handler) override {
				_cpu.exception_sigh(handler); }

			Affinity::Space affinity_space() const override {
				return _cpu.affinity_space(); }

			Dataspace_capability trace_control() override
			{
				if (!_trace_control.valid()) {
					_trace_control = _cpu.trace_control();
					new (env()->heap()) Static_dataspace_info(_registry,
					                                          _trace_control);
				}
				return _trace_control;
			}

			Quota quota() override { return _cpu.quota(); }

			int ref_account(Cpu_session_capability c) override {
				return _cpu.ref_account(c); }

			int transfer_quota(Cpu_session_capability c, size_t q) override {
				return _cpu.transfer_quota(c, q); }

			Capability<Native_cpu> native_cpu() override {
				return _cpu.native_cpu(); }
	};
}

#endif /* _NOUX__CPU_SESSION_COMPONENT_H_ */
