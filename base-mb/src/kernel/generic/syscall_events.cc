/*
 * \brief  Syscall handling implementation
 * \author Martin Stein
 * \date   2011-02-22
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <generic/syscall_events.h>
#include "include/thread.h"
#include <generic/verbose.h>
#include <generic/printf.h>

using namespace Kernel;

namespace Kernel{
	extern Thread* idle_thread();
}


Print_char::On_occurence__result Print_char::on_print_char(char c)
{
	printf("%c", c);
	return EVENT_PROCESSED;
}


Thread_create::On_occurence__result
Thread_create::on_thread_create(Argument* a, Result* r)
{
	using namespace Thread_create_types;

	if (!_permission_to_do_thread_create()) {
		_on_thread_create__warning__failed();
		*r = INSUFFICIENT_PERMISSIONS;
	} else {
		Thread *t = thread_factory()->create(a, false);

		if (!t) {
			_on_thread_create__warning__failed();
			*r=INAPPROPRIATE_THREAD_ID;
		} else{
			scheduler()->add(t);
			_on_thread_create__verbose__success(t);
			*r=SUCCESS;
		}
	}
	return EVENT_PROCESSED;
}


Thread_kill::On_occurence__result
Thread_kill::on_thread_kill(Argument *a, Result *r)
{
	using namespace Thread_kill_types;

	if (!_permission_to_do_thread_kill()) {
		*r = INSUFFICIENT_PERMISSIONS;
		_on_thread_kill__warning__failed();
	} else {
		Thread_factory *tf = thread_factory();

		if (tf->get(a->tid) == this) {
			*r = SUICIDAL;
			_on_thread_kill__warning__failed();
		} else {
			if(tf->kill(a->tid)){
				printf("Warning in Thread_kill::on_thread_kill: Can't kill thread\n");
			};
			*r=SUCCESS;
			_on_thread_kill__verbose__success(a->tid);
		}
	}
	return EVENT_PROCESSED;
}


Thread_sleep::On_occurence__result Thread_sleep::on_thread_sleep()
{
	Scheduler *s = scheduler();

	s->remove(s->current_client());
	_on_thread_sleep__verbose__success();

	return EVENT_PROCESSED;
}


Thread_wake::On_occurence__result
Thread_wake::on_thread_wake(Argument *a, Result *r)
{
	using namespace Thread_wake_types;
	Thread* t = thread_factory()->get(a->tid);

	if (!t) {
		*r = INAPPROPRIATE_THREAD_ID;
		_on_thread_wake__warning__failed();
	} else{
		if (!_permission_to_do_thread_wake(t)) {
			*r = INSUFFICIENT_PERMISSIONS;
			_on_thread_wake__warning__failed();
		} else {
			scheduler()->add(t);
			*r = SUCCESS;
			_on_thread_wake__verbose__success(a->tid);
		}
	}
	return EVENT_PROCESSED;
}


//void Print_info::on_print_info(){
//	_print_info();
//}


void Tlb_flush::on_tlb_flush(Paging::Virtual_page* first_page, unsigned size)
{
	if (!_permission_to_do_tlb_flush()) return;

	tlb()->flush(first_page, size);
}


void Tlb_load::on_tlb_load(Paging::Resolution* r)
{
	if (!_permission_to_do_tlb_load()) return;

	tlb()->add(r);
}


void Thread_pager::on_thread_pager(Thread_id target_tid, Thread_id pager_tid)
{
	if (!_permission_to_do_thread_pager(target_tid)) {
		printf("Warning in Kernel::Thread_pager::on_thread_pager, "
		       "insufficient permissions\n");
		return;
	}

	Thread *target = thread_factory()->get(target_tid);
	if (target && target != idle_thread()) {
		target->pager_tid(pager_tid);
	} else {
		printf("Warning in Kernel::Thread_pager::on_thread_pager, "
		       "invalid target thread id\n");
	}
}


void Thread_wake::_on_thread_wake__verbose__success(Thread_id tid)
{
	if (!SYSCALL_EVENT__VERBOSE) return;

	printf("Kernel::Thread_wake::on_thread_wake, success, tid=%i\n", tid);
}


void Thread_sleep::_on_thread_sleep__verbose__success()
{
	if (!SYSCALL_EVENT__VERBOSE) return;

	printf("Kernel::Thread_sleep::on_thread_sleep, success\n");
}


void Thread_kill::_on_thread_kill__verbose__success(Thread_id tid)
{
	if (!SYSCALL_EVENT__VERBOSE) return;

	printf("Kernel::Thread_kill::on_thread_kill, success, tid=%i\n", tid);
}


void Thread_create::_on_thread_create__verbose__success(Thread* t)
{
	if (!SYSCALL_EVENT__VERBOSE) return;

	printf("Kernel::Thread_create::on_thread_create, success, printing constraints\n");
	t->print_state();
}

