/*
 * \brief  Kernels Userland Thread representation
 * \author Martin stein
 * \date   2010-06-24
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "include/thread.h"


using namespace Kernel;

extern bool trace_me;

Thread::Thread(Constructor_argument* a)
:
	Syscall::Source(a->utcb, a->tid),
	_platform_thread(a->vip, a->vsp, a->pid, this),
	_id(a->tid),
	_is_privileged(a->is_privileged),
	_pager_tid(a->pager_tid),
	_substitute_tid(INVALID_THREAD_ID),
	_state(READY),
	_waits_for_irq(false),
	_any_irq_pending(false)
{
	_platform_thread.bootstrap_argument_0( (word_t)utcb() );

	Instruction_tlb_miss::Listener::_event(_platform_thread.exception()->instruction_tlb_miss());
	Data_tlb_miss::Listener::_event(_platform_thread.exception()->data_tlb_miss());

	_constructor__verbose__success();
}


Thread::Thread() : Syscall::Source(0, 0)
{
	_invalidate();
}


void Thread::print_state()
{
	printf("Thread ID: %i, pager: %i, substitute: %i, privileged: %c, state: %i\n"
	       "Context:\n",
	       _id, _pager_tid, _substitute_tid, _is_privileged ? 'y' : 'n', 
	       _state);
	_platform_thread.print_state();
}


void Thread::_on_data_tlb_miss(Virtual_page *accessed_page, bool write_access)
{
	typedef Kernel::Data_tlb_miss::Listener Listener;
	using namespace Paging;

	if (_protection_id()==Roottask::PROTECTION_ID & !_pager_tid) {

		Listener::_resolve_identically((Physical_page::size_t)ROOTTASK_PAGE_SIZE,
		                               Physical_page::RW);
		_on_data_tlb_miss__verbose__roottask_resolution(accessed_page->address());

	} else {
		Ipc::Participates_dialog * pager = thread_factory()->get(_pager_tid);
		if (!pager) {
			_on_data_tlb_miss__warning__invalid_pager_tid(_pager_tid);
			return;
		} else {
			Request::Source s = {_id, _instruction_pointer() };
			Request::Access a = write_access ? Request::RW : Request::R;

			_paging_request = Request(accessed_page, s, a);
			_send_message(pager, (void*)&_paging_request,
			              sizeof(_paging_request));

			_sleep();
			_unblock();
		}
	}
}


void Thread::_on_instruction_tlb_miss(Virtual_page *accessed_page)
{
	typedef Kernel::Instruction_tlb_miss::Listener Listener;
	using namespace Paging;

	if (_protection_id() == Roottask::PROTECTION_ID & !_pager_tid) {

		Listener::_resolve_identically((Physical_page::size_t)ROOTTASK_PAGE_SIZE,
		                               Physical_page::RX);

		_on_instruction_tlb_miss__verbose__roottask_resolution(accessed_page->address());

	} else {
		Ipc::Participates_dialog *pager = thread_factory()->get(_pager_tid);
		if (!pager) {
			_on_data_tlb_miss__warning__invalid_pager_tid(_pager_tid);
			return;
		} else{
			Paging::Request::Source s = {_id, _instruction_pointer()};
			_paging_request = Request(accessed_page, s, Request::RX);

			_send_message(pager, (void*)&_paging_request,
			                         sizeof(_paging_request));
			_sleep();
			_unblock();
		}
	}
}

