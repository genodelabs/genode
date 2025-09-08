/*
 * \brief   Userland interface for the management of kernel thread-objects
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__PLATFORM_THREAD_H_
#define _CORE__PLATFORM_THREAD_H_

/* Genode includes */
#include <base/ram_allocator.h>
#include <base/thread.h>
#include <base/trace/types.h>
#include <base/rpc_server.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>

/* core includes */
#include <address_space.h>
#include <object.h>
#include <dataspace_component.h>

/* kernel includes */
#include <kernel/core_interface.h>
#include <kernel/thread.h>

namespace Core {

	class Pager_object;
	class Rm_client;
	class Platform_thread;
	class Platform_pd;
}


namespace Genode { class Thread_state; }


class Core::Platform_thread : Noncopyable
{
	private:

		/*
		 * Noncopyable
		 */
		Platform_thread(Platform_thread const &);
		Platform_thread &operator = (Platform_thread const &);

		using Label = String<32>;

		struct Utcb : Noncopyable
		{
			struct {
				Ram_allocator *_ram_ptr      = nullptr;
				Local_rm      *_local_rm_ptr = nullptr;
			};

			Ram_allocator::Result const ds; /* UTCB ds of non-core threads */

			addr_t const core_addr; /* UTCB address within core/kernel */
			addr_t const phys_addr;

			addr_t _attach(Local_rm &);

			static addr_t _phys(Rpc_entrypoint &ep, Dataspace_capability ds)
			{
				return ep.apply(ds, [&] (Dataspace_component *dsc) {
					return dsc ? dsc->phys_addr() : 0; });
			}

			static addr_t _ds_phys(Rpc_entrypoint &ep, Ram_allocator::Result const &ram)
			{
				return ram.convert<addr_t>(
					[&] (Ram::Allocation const &ds) { return _phys(ep, ds.cap); },
					[&] (Ram::Error)                { return 0UL; });
			}

			/**
			 * Constructor used for core-local threads
			 */
			Utcb(addr_t core_addr);

			/**
			 * Constructor used for threads outside of core
			 */
			Utcb(Rpc_entrypoint &ep, Ram_allocator &ram, Local_rm &local_rm)
			:
				_local_rm_ptr(&local_rm),
				ds(ram.try_alloc(sizeof(Native_utcb), CACHED)),
				core_addr(_attach(local_rm)),
				phys_addr(_ds_phys(ep, ds))
			{ }

			~Utcb()
			{
				if (_local_rm_ptr)
					_local_rm_ptr->detach(core_addr);
			}

			Ram_dataspace_capability ds_cap() const
			{
				return ds.convert<Ram_dataspace_capability>(
					[&] (Ram::Allocation const &ds) { return ds.cap; },
					[&] (Ram::Error) { return Ram_dataspace_capability { }; });
			}
		};

		Label              const _label;
		Platform_pd             &_pd;
		Weak_ptr<Address_space>  _address_space { };
		Pager_object *           _pager;
		Utcb                     _utcb;
		unsigned                 _group_id { 0 };

		/*
		 * Wether this thread is the main thread of a program.
		 * This should be used only after 'join_pd' was called
		 * or if this is a core thread. For core threads its save
		 * also without 'join_pd' because '_main_thread' is initialized
		 * with 0 wich is always true as cores main thread has no
		 * 'Platform_thread'.
		 */
		bool _main_thread;

		Affinity::Location _location;

		Kernel_object<Kernel::Thread> _kobj;

		/**
		 * Common construction part
		 */
		void _init();

		/*
		 * Check if this thread will attach its UTCB by itself
		 */
		bool _attaches_utcb_by_itself();

		unsigned _scale_priority(unsigned virt_prio)
		{
			static constexpr unsigned p =
				Kernel::Scheduler::Group_id::BACKGROUND+1;
			return Cpu_session::scale_priority(p, virt_prio, false);
		}

		Platform_pd &_kernel_main_get_core_platform_pd();

	public:

		using Constructed = Attempt<Ok, Alloc_error>;

		Constructed const constructed = _utcb.ds.convert<Constructed>(
			[] (auto &)        { return Ok(); },
			[] (Alloc_error e) { return e; });

		/**
		 * Constructor for core threads
		 *
		 * \param label       debugging label
		 * \param utcb        virtual address of UTCB within core
		 * \param location    targeted location in affinity space
		 */
		Platform_thread(Label const &label, Native_utcb &utcb, Affinity::Location);

		/**
		 * Constructor for threads outside of core
		 *
		 * \param label      debugging label
		 * \param virt_prio  unscaled processor-scheduling priority
		 * \param utcb       core local pointer to userland stack
		 */
		Platform_thread(Platform_pd &, Rpc_entrypoint &, Ram_allocator &,
		                Local_rm &, Label const &label,
		                unsigned const virt_prio, Affinity::Location,
		                addr_t const utcb);

		/**
		 * Destructor
		 */
		~Platform_thread();

		/**
		 * Return information about current exception state
		 *
		 * This syscall wrapper is located here and not in
		 * 'core_interface.h' because the 'Thread::Exception_state'
		 * type is not known there.
		 */
		Kernel::Thread::Exception_state exception_state()
		{
			Kernel::Thread::Exception_state exception_state;
			using namespace Kernel;
			call(call_id_exception_state(), (Call_arg)&*_kobj,
			                                (Call_arg)&exception_state);
			return exception_state;
		}

		/**
		 * Return information about current fault
		 */
		Kernel::Thread_fault fault_info() { return _kobj->fault(); }

		/**
		 * Run this thread
		 *
		 * \param ip  initial instruction pointer
		 * \param sp  initial stack pointer
		 */
		void start(void *ip, void *sp);

		void restart();

		void fault_resolved(Untyped_capability, bool);

		/**
		 * Pause this thread
		 */
		void pause() { Kernel::pause_thread(*_kobj); }

		/**
		 * Enable/disable single stepping
		 */
		void single_step(bool on) { Kernel::single_step(*_kobj, on); }

		/**
		 * Resume this thread
		 */
		void resume()
		{
			if (exception_state() !=
			    Kernel::Thread::Exception_state::NO_EXCEPTION)
				restart();

			Kernel::resume_thread(*_kobj);
		}

		/**
		 * Get raw thread state
		 */
		Thread_state state();

		/**
		 * Override raw thread state
		 */
		void state(Thread_state s);

		/**
		 * Return unique identification of this thread as faulter
		 */
		unsigned long pager_object_badge() { return (unsigned long)this; }

		/**
		 * Set the executing CPU for this thread
		 *
		 * \param location  targeted location in affinity space
		 */
		void affinity(Affinity::Location const &location);

		/**
		 * Get the executing CPU for this thread
		 */
		Affinity::Location affinity() const;

		/**
		 * Return the address space to which the thread is bound
		 */
		Weak_ptr<Address_space>& address_space();

		/**
		 * Return execution time consumed by the thread
		 */
		Trace::Execution_time execution_time() const
		{
			uint64_t execution_time =
				const_cast<Platform_thread *>(this)->_kobj->execution_time();
			return { execution_time, 0, 0, _group_id }; }


		/***************
		 ** Accessors **
		 ***************/

		Label label() const { return _label; };

		void pager(Pager_object &pager);

		Pager_object &pager();

		Platform_pd &pd() const { return _pd; }

		Ram_dataspace_capability utcb() const { return _utcb.ds_cap(); }
};

#endif /* _CORE__PLATFORM_THREAD_H_ */
