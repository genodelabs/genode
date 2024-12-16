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
				Ram_allocator *_ram_ptr     = nullptr;
				Region_map    *_core_rm_ptr = nullptr;
			};

			Ram_dataspace_capability _ds { }; /* UTCB ds of non-core threads */

			addr_t const core_addr; /* UTCB address within core/kernel */
			addr_t const phys_addr;

			/*
			 * \throw Out_of_ram
			 * \throw Out_of_caps
			 */
			Ram_dataspace_capability _allocate(Ram_allocator &ram)
			{
				return ram.alloc(sizeof(Native_utcb), CACHED);
			}

			addr_t _attach(Region_map &);

			static addr_t _ds_phys(Rpc_entrypoint &ep, Dataspace_capability ds)
			{
				return ep.apply(ds, [&] (Dataspace_component *dsc) {
					return dsc ? dsc->phys_addr() : 0; });
			}

			/**
			 * Constructor used for core-local threads
			 */
			Utcb(addr_t core_addr);

			/**
			 * Constructor used for threads outside of core
			 */
			Utcb(Rpc_entrypoint &ep, Ram_allocator &ram, Region_map &core_rm)
			:
				_core_rm_ptr(&core_rm),
				_ds(_allocate(ram)),
				core_addr(_attach(core_rm)),
				phys_addr(_ds_phys(ep, _ds))
			{ }

			~Utcb()
			{
				if (_core_rm_ptr)
					_core_rm_ptr->detach(core_addr);

				if (_ram_ptr && _ds.valid())
					_ram_ptr->free(_ds);
			}
		};

		Label              const _label;
		Platform_pd             &_pd;
		Weak_ptr<Address_space>  _address_space  { };
		Pager_object *           _pager;
		Utcb                     _utcb;
		unsigned                 _priority       {0};
		unsigned                 _quota          {0};

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
			static constexpr unsigned p = Kernel::Scheduler::Priority::max();
			return Cpu_session::scale_priority(p, virt_prio);
		}

		Platform_pd &_kernel_main_get_core_platform_pd();

	public:

		/**
		 * Constructor for core threads
		 *
		 * \param label       debugging label
		 * \param utcb        virtual address of UTCB within core
		 */
		Platform_thread(Label const &label, Native_utcb &utcb);

		/**
		 * Constructor for threads outside of core
		 *
		 * \param quota      CPU quota that shall be granted to the thread
		 * \param label      debugging label
		 * \param virt_prio  unscaled processor-scheduling priority
		 * \param utcb       core local pointer to userland stack
		 */
		Platform_thread(Platform_pd &, Rpc_entrypoint &, Ram_allocator &,
		                Region_map &, size_t const quota, Label const &label,
		                unsigned const virt_prio, Affinity::Location,
		                addr_t const utcb);

		/**
		 * Destructor
		 */
		~Platform_thread();

		/**
		 * Return true if thread creation succeeded
		 */
		bool valid() const { return true; }

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
		 * Set CPU quota of the thread to 'quota'
		 */
		void quota(size_t const quota);

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
		void affinity(Affinity::Location const & location);

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
			return { execution_time, 0, _quota, _priority }; }


		/***************
		 ** Accessors **
		 ***************/

		Label label() const { return _label; };

		void pager(Pager_object &pager);

		Pager_object &pager();

		Platform_pd &pd() const { return _pd; }

		Ram_dataspace_capability utcb() const { return _utcb._ds; }
};

#endif /* _CORE__PLATFORM_THREAD_H_ */
