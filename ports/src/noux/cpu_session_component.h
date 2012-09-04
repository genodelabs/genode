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
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__CPU_SESSION_COMPONENT_H_
#define _NOUX__CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <cpu_session/connection.h>

namespace Noux {

	using namespace Genode;

	class Cpu_session_component : public Rpc_object<Cpu_session>
	{
		private:

			bool const        _forked;
			Cpu_connection    _cpu;
			Thread_capability _main_thread;

		public:

			/**
			 * Constructor
			 *
			 * \param forked  false if the CPU session belongs to a child
			 *                created via execve or to the init process, or
			 *                true if the CPU session belongs to a newly forked
			 *                process.
			 *
			 * The 'forked' parameter controls the policy applied to the
			 * startup of the main thread.
			 */
			Cpu_session_component(char const *label, bool forked)
			: _forked(forked), _cpu(label) { }

			/**
			 * Explicitly start main thread, only meaningful when
			 * 'forked' is true
			 */
			void start_main_thread(addr_t ip, addr_t sp)
			{
				_cpu.start(_main_thread, ip, sp);
			}


			/***************************
			 ** Cpu_session interface **
			 ***************************/

			Thread_capability create_thread(Name const &name, addr_t utcb)
			{
				/*
				 * Prevent any attempt to create more than the main
				 * thread.
				 */
				if (_main_thread.valid()) {
					PWRN("Invalid attempt to create a thread besides main");
					while (1);
					return Thread_capability();
				}
				_main_thread = _cpu.create_thread(name, utcb);

				PINF("created main thread");
				return _main_thread;
			}

			Ram_dataspace_capability utcb(Thread_capability thread) {
				return _cpu.utcb(thread); }

			void kill_thread(Thread_capability thread) {
				_cpu.kill_thread(thread); }

			Thread_capability first() {
				return _cpu.first(); }

			Thread_capability next(Thread_capability curr) {
				return _cpu.next(curr); }

			int set_pager(Thread_capability thread,
			              Pager_capability  pager) {
			    return _cpu.set_pager(thread, pager); }

			int start(Thread_capability thread, addr_t ip, addr_t sp)
			{
				if (_forked) {
					PINF("defer attempt to start thread at ip 0x%lx", ip);
					return 0;
				}
				return _cpu.start(thread, ip, sp);
			}

			void pause(Thread_capability thread) {
				_cpu.pause(thread); }

			void resume(Thread_capability thread) {
				_cpu.resume(thread); }

			void cancel_blocking(Thread_capability thread) {
				_cpu.cancel_blocking(thread); }

			int state(Thread_capability thread, Thread_state *dst) {
				return _cpu.state(thread, dst); }

			void exception_handler(Thread_capability         thread,
			                       Signal_context_capability handler) {
			    _cpu.exception_handler(thread, handler); }

			void single_step(Thread_capability thread, bool enable) {
				_cpu.single_step(thread, enable); }

			unsigned num_cpus() const {
				return _cpu.num_cpus(); }

			void affinity(Thread_capability thread, unsigned cpu) {
				_cpu.affinity(thread, cpu); }
	};
}

#endif /* _NOUX__CPU_SESSION_COMPONENT_H_ */
