/*
 * \brief  Blockings that can prevent a thread from beeing executed
 * \author Martin Stein
 * \date   2011-02-22
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <generic/blocking.h>
#include <generic/timer.h>
#include <generic/event.h>
#include "include/thread.h"
#include <generic/irq_controller.h>
#include <kernel/syscalls.h>

Kernel::Data_tlb_miss::On_occurence__result Kernel::Data_tlb_miss::on_occurence()
{
	if (!_missing_resolution.virtual_page.valid()) {
		_on_occurence__error__virtual_page_invalid(); }

	Event::_populate();
	if (!_missing_resolution.physical_page.valid()) {
		_on_occurence__verbose__waiting_for_resolution();
		return EVENT_PENDING; }

	tlb()->add(&_missing_resolution);
	_missing_resolution.invalidate();
	return EVENT_PROCESSED;
}


void Kernel::Data_tlb_miss::Listener::_resolve_identically(Physical_page::size_t        s,
                                                           Physical_page::Permissions p)
{
	new (&_resolution->physical_page)
	Physical_page(_resolution->virtual_page.address(), s, p);
}


Kernel::Instruction_tlb_miss::On_occurence__result Kernel::Instruction_tlb_miss::on_occurence()
{
	if (!_missing_resolution.virtual_page.valid())
		_on_occurence__error__virtual_page_invalid();

	Event::_populate();
	if (!_missing_resolution.physical_page.valid()) {
		_on_occerence__verbose__waiting_for_resolution();
		return EVENT_PENDING;
	}

	tlb()->add(&_missing_resolution);
	_missing_resolution.invalidate();
	return EVENT_PROCESSED;
}


void Kernel::Instruction_tlb_miss::Listener::_resolve_identically(Physical_page::size_t        s,
                                                                  Physical_page::Permissions p)
{
	new (&_resolution->physical_page)
	Physical_page(_resolution->virtual_page.address(), s, p);
}


void Kernel::Data_tlb_miss::Listener::_on_event()
{
	_on_data_tlb_miss(&_resolution->virtual_page,
	                   _resolution->write_access);
}


void Kernel::Instruction_tlb_miss::Listener::_on_event()
{
	_on_instruction_tlb_miss(&_resolution->virtual_page);
}

extern bool irq_occured[];

bool Kernel::Irq::unblock()
{
	Thread * const h = irq_allocator()->holder(_id);
	if (!h) {
		irq_controller()->ack_irq(_id);
		return true;
	}
	h->handle(_id);
	return true;
}


bool Kernel::Exception::unblock()
{
	int result = false;

	switch (_id) {

	case INSTRUCTION_TLB_MISS:

		new (&Instruction_tlb_miss::_missing_resolution.virtual_page)
			Virtual_page(address(), protection_id());

		if (Instruction_tlb_miss::on_occurence() == EVENT_PROCESSED)
			result = true;

		break;

	case DATA_TLB_MISS:

		new (&Data_tlb_miss::_missing_resolution.virtual_page)
			Virtual_page(address(), protection_id());

		Data_tlb_miss::_missing_resolution.write_access = attempted_write_access();

		if (Data_tlb_miss::on_occurence() == EVENT_PROCESSED)
			result = true;

		break;

	default:
		PERR("Unexpected exception %i\n", _id);
		halt();
	}

	return result;
}


bool Kernel::Syscall::unblock()
{
	switch (_id){
	case PRINT_CHAR:
		{
			return _source->on_print_char(*_argument_0) == Event::EVENT_PROCESSED ?
			       true : false;
		}

	case THREAD_CREATE:
		{
			Thread_create::Argument a;
			a.tid           =     (Thread_id)*_argument_0;
			a.pid           = (Protection_id)*_argument_1;
			a.pager_tid     =     (Thread_id)*_argument_2;
			a.utcb          =         (Utcb*)*_argument_3;
			a.vip           =          (addr_t)*_argument_4;
			a.vsp           =          (addr_t)*_argument_5;
			a.is_privileged =
				(bool)(*_argument_6&(1<<THREAD_CREATE_PARAMS_ROOTRIGHT_LSHIFT));

			return _source->on_thread_create(&a, (Thread_create::Result*)_result_0) == Event::EVENT_PROCESSED ?
			       true : false;
		}

	case THREAD_KILL:
		{
			Thread_kill::Argument a;
			a.tid = (Thread_id)*_argument_0;

			return _source->on_thread_kill(&a, (Thread_kill::Result*)_result_0) == Event::EVENT_PROCESSED ?
			       true : false;
		}

	case THREAD_WAKE:
		{
			Thread_wake::Argument a;
			a.tid = (Thread_id)*_argument_0;

			return _source->on_thread_wake(&a, (Thread_wake::Result*)_result_0) == Event::EVENT_PROCESSED ?
			       true : false;
		}

	case THREAD_SLEEP:
		{
			return _source->on_thread_sleep() == Event::EVENT_PROCESSED ?
			       true : false;
		}

	case IPC_SERVE:
		{
			return _source->can_reply_and_get_next_request(*_argument_0, _argument_0);
		}

	case IPC_REQUEST:
		{
			return _source->can_get_reply(thread_factory()->get(*_argument_0),
			                                                    *_argument_1, 
			                                                    _argument_0);
		}

	case TLB_LOAD:
		{
			using namespace Paging;

			Virtual_page vp((addr_t)         *_argument_1,
			                (Protection_id)*_argument_2);

			Physical_page pp((addr_t)                      *_argument_0,
			                 (Physical_page::size_t)       *_argument_3,
			                 (Physical_page::Permissions)*_argument_4);

			Paging::Resolution r(&vp, &pp);
			_source->on_tlb_load(&r);
			return true;
		}

	case IRQ_ALLOCATE:
		{
			return _source->irq_allocate((Irq_id)*_argument_0, (int *)_argument_0);
		}

	case IRQ_FREE:
		{
			return _source->irq_free((Irq_id)*_argument_0, (int *)_argument_0);
		}

	case IRQ_WAIT:
		{
			return _source->irq_wait();
		}

	case THREAD_PAGER:
		{
			_source->on_thread_pager(*_argument_0, *_argument_1);
			return true;
		}

	case THREAD_YIELD:
		{
			_source->yield();
			return true;
		}

	case TLB_FLUSH:
		{
			using namespace Paging;

			Virtual_page vp((addr_t)         *_argument_1,
			                (Protection_id)*_argument_0);
			_source->on_tlb_flush(&vp, (unsigned)*_argument_2);
			return true;
		}

		case PRINT_INFO:
		{
			Thread* t;
			if((Thread_id)*_argument_0) {
				t=thread_factory()->get((Thread_id)*_argument_0);
			} else {
				t=thread_factory()->get(_source->tid);
			}
			if(!t) { return true; }
			t->print_state();
			return true;
		}

	default:
		{
			_unblock__warning__unknown_id();
			return false;
		}
	}
}


