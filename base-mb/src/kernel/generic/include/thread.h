/*
 * \brief  Declaration of physical backend to userland thread
 * \author Martin stein
 * \date   2010-06-24
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__GENERIC__INCLUDE__THREAD_H_
#define _KERNEL__GENERIC__INCLUDE__THREAD_H_


#include <kernel/types.h>
#include <kernel/syscalls.h>
#include <generic/blocking.h>

#include <generic/scheduler.h>
#include <util/queue.h>
#include <generic/verbose.h>
#include <platform/platform.h>
#include <generic/tlb.h>

#include <generic/syscall_events.h>

extern bool irq_occured[Kernel::MAX_IRQ_ID];

namespace Kernel {

	enum { THREAD__VERBOSE = 0,
	       THREAD__WARNING = 1 };

	enum { ROOTTASK_PAGE_SIZE = Paging::Physical_page::_4KB };


	class Thread : public Scheduler::Client,
	               public Instruction_tlb_miss::Listener,
	               public Data_tlb_miss::Listener,
	               public Syscall::Source,
	               public Paging::Request::Source
	{
		public:

			typedef Tlb::Virtual_page       Virtual_page;
			typedef Tlb::Physical_page      Physical_page;
			typedef Tlb::Resolution         Resolution;
			typedef Thread_create::Argument Constructor_argument;
			typedef Kernel::Thread_id       Thread_id;
			typedef Kernel::Protection_id   Protection_id;

			void* operator new(size_t, void *addr) { return addr; }

			enum State { INVALID = 0,
			             READY,
			             WAIT,
			             WAIT_IPC_REPLY,
			             WAIT_IPC_REQUEST };

			enum Print_mode { BRIEF_STATE, DETAILED_STATE };

		private:

			Platform_thread _platform_thread;
			Thread_id       _id;
			bool            _is_privileged;
			Thread_id       _pager_tid;
			Thread_id       _substitute_tid;
			State           _state;
			Paging::Request _paging_request;
			bool            _waits_for_irq;
			bool            _any_irq_pending;
			bool            _irq_pending[Kernel::MAX_IRQ_ID];

			void _unblock() { _platform_thread.unblock(); }

			void _invalidate() { _state = INVALID; }

			Protection_id _protection_id() { return _platform_thread.protection_id(); }

			addr_t _instruction_pointer() { return _platform_thread.instruction_pointer(); }

			void _sleep() { scheduler()->remove(this); }

			void _yield_after_atomic_operation()
			{
				_platform_thread.yield_after_atomic_operation();
			}

			inline void _clear_pending_irqs();

		public:

			inline bool irq_allocate(Irq_id i, int * const result);
			inline bool irq_free(Irq_id i, int * const result);
			inline bool irq_wait();
			inline void handle(Irq_id const & i);

			void pager_tid(Thread_id ptid){ _pager_tid=ptid; }

			/**
			 * Constructor
			 */
			Thread(Constructor_argument* a);

			/**
			 * Constructing invalid thread without parameters
			 */
			Thread();

			/**
			 * Shows several infos about thread depending on print mode argument
			 */
			void print_state();


			/**********************
			 ** Simple Accessors **
			 **********************/

			Thread_id thread_id() { return _id; }
			bool valid() { return _state != INVALID; }


			/*********************************
			 ** Scheduler::Client interface **
			 *********************************/

			int label() { return (int)_id; }

			void _on_instruction_tlb_miss(Virtual_page* accessed_page);

			void _on_data_tlb_miss(Virtual_page* accessed_page, bool write_access);

			void yield() { scheduler()->skip_next_time(this); }

		protected:

			void ipc_sleep() { Scheduler::Client::_sleep(); }

			void ipc_wake() { Scheduler::Client::_wake(); }

			bool _preemptable()
			{
				if (!platform()->is_atomic_operation((void*)_instruction_pointer()))
					return true;

				_yield_after_atomic_operation();
				return false;
			}

			bool _permission_to_do_print_char() { return true; }

			bool _permission_to_do_thread_create()
			{
				return _is_privileged;
			}

			bool _permission_to_do_thread_kill()
			{
				return _is_privileged;
			}

//			bool _print_info(){
//				_platform_thread.exec_context()->print_content(2);
//				return true;
//			}

		bool _permission_to_do_tlb_load(){ return _is_privileged; }

		bool _permission_to_do_tlb_flush(){ return _is_privileged; }

		bool _permission_to_do_thread_pager(Thread_id target_tid)
		{
			return _is_privileged;
		}

		bool _permission_to_do_thread_wake(Thread* target)
		{
			return _is_privileged
			    || target->_protection_id()==_protection_id();
		}

		Context *_context()
		{
			return _platform_thread.unblocked_exec_context();
		}


		enum { CONSTRUCTOR__VERBOSE__SUCCESS = THREAD__VERBOSE };

		void _on_data_tlb_miss__warning__invalid_pager_tid(Thread_id pager_tid)
		{
			if (!THREAD__WARNING) return;

			printf("Warning in Kernel::Thread::_on_data_tlb_miss, invalid pager_tid=%i\n", pager_tid);
		}

		void _constructor__verbose__success()
		{
			if (!CONSTRUCTOR__VERBOSE__SUCCESS) return;

			printf("Kernel::Thread::Thread, new valid thread created, printing state\n");
			Verbose::indent(2);
			printf("_utcb=0x%8X, _platform_thread(", (uint32_t)utcb());
			_platform_thread.print_state();
			printf(")\n");
		}

		void _on_instruction_tlb_miss__verbose__roottask_resolution(addr_t v)
		{
			if (!THREAD__VERBOSE) return;

			printf("Kernel::Thread::_on_instruction_tlb_miss, resoluted 0x%p identically\n",
			       (void*)v);
		}

		void _on_data_tlb_miss__verbose__roottask_resolution(addr_t v)
		{
			if (!THREAD__VERBOSE) return;

			printf("Kernel::Thread::_on_data_tlb_miss, resoluted 0x%p identically\n",
			       (void*)v);
		}
	};


	class Thread_factory
	{
		enum { THREAD_ID_RANGE = 1 << (Platform::BYTE_WIDTH*sizeof(Thread_id)) };

		Thread thread[THREAD_ID_RANGE];
		bool _steady[THREAD_ID_RANGE];

		public:

			enum Error { 
				NO_ERROR = 0,
				CANT_KILL_STEADY_THREAD = -1
			};

			typedef Thread::Constructor_argument Create_argument;

			Thread *create(Create_argument *a, bool steady)
			{
				if(thread[a->tid].valid()) { return 0; }
				_steady[a->tid] = steady;
				return new(&thread[a->tid])Thread(a);
			}

			Error kill(Thread_id tid)
			{
				if(_steady[tid]) return CANT_KILL_STEADY_THREAD;
				thread[tid].~Thread();
				new (&thread[tid]) Thread();
				return NO_ERROR;
			}

			Thread *get(Thread_id id)
			{
				return thread[id].valid() ? &thread[id] : (Thread*)0;
			}
	};


	Thread_factory* thread_factory();
}


void Kernel::Thread::handle(Irq_id const & i)
{
	if(i>sizeof(_irq_pending)/sizeof(_irq_pending[0])){
		printf("Kernel::Thread::handles(Irq_id i): Error");
		halt();
	}
	_irq_pending[i]=true;
	_any_irq_pending = true;

	if(!_waits_for_irq) { return; }

	Scheduler::Client::_wake();
	_clear_pending_irqs();
	_waits_for_irq=false;
	return;
}


bool Kernel::Thread::irq_allocate(Irq_id i, int * const result)
{
	if(!_is_privileged){ 
		*result = -1;
		return true;
	};

	if(i == platform()->timer()->irq_id()){ 
		*result = -3;
		return true;
	};

	if(irq_allocator()->allocate(this, i)){
		*result = -2;
		return true;
	};

	*result = 0;
	return true;
}


bool Kernel::Thread::irq_free(Irq_id i, int * const result)
{
	if(!_is_privileged){ 
		*result = -1;
		return true;
	};

	if(irq_allocator()->free(this, i)){
		*result = -2;
		return true;
	};

	*result = 0;
	return true;
}


bool Kernel::Thread::irq_wait()
{
	if(!_any_irq_pending){
		_waits_for_irq = true;
		Scheduler::Client::_sleep();
		return true;
	}
	_clear_pending_irqs();
	return true;
}


void Kernel::Thread::_clear_pending_irqs()
{
	for(unsigned int i=0; i<sizeof(_irq_pending)/sizeof(_irq_pending[0]); i++) {
		if(_irq_pending[i]) {
			irq_controller()->ack_irq(i);
			_irq_pending[i]=false;
		}
	}
	_any_irq_pending=false;
}

#endif /* _KERNEL__GENERIC__INCLUDE__THREAD_H_ */







