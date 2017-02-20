/*
 * \brief   Pistachio thread facility
 * \author  Julian Stecklina
 * \date    2008-03-19
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

/* core includes */
#include <platform_pd.h>
#include <platform_thread.h>
#include <kip.h>
#include <print_l4_thread_id.h>

/* Pistachio includes */
namespace Pistachio
{
#include <l4/types.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/sigma0.h>
};

using namespace Genode;
using namespace Pistachio;

static const bool verbose = false;
static const bool verbose2 = true;


void Platform_thread::affinity(Affinity::Location location)
{
	_location = location;

	unsigned const cpu_no = location.xpos();

	if (cpu_no >= L4_NumProcessors(get_kip())) {
		error("invalid processor number");
		return;
	}

	if (_l4_thread_id != L4_nilthread)
		if (L4_Set_ProcessorNo(_l4_thread_id, cpu_no) == 0)
			error("could not set processor number");
}


Affinity::Location Platform_thread::affinity() const
{
	return _location;
}


int Platform_thread::start(void *ip, void *sp)
{
	L4_ThreadId_t thread = _l4_thread_id;
	L4_ThreadId_t pager  = _pager
	                     ? Capability_space::ipc_cap_data(_pager->cap()).dst
	                     : L4_nilthread;

	/* XXX should always be the root task */
	L4_ThreadId_t preempter = L4_Myself();

	if (_thread_id == THREAD_INVALID) {
		error("attempt to start a thread with invalid ID");
		return -1;
	}

	L4_Word_t utcb_location = _platform_pd->_utcb_location(_thread_id);

	int ret = L4_ThreadControl(thread, _platform_pd->_l4_task_id,
	                           preempter, L4_Myself(), (void *)utcb_location);

	if (ret != 1) {
		error(__func__, ": L4_ThreadControl returned ", Hex(L4_ErrorCode()));
		return -2;
	}

	/* set real pager */
	ret = L4_ThreadControl(thread, _platform_pd->_l4_task_id,
	                       L4_nilthread, pager, (void *)-1);

	if (ret != 1) {
		error(__func__, ": L4_ThreadControl returned ", Hex(L4_ErrorCode()));
		error("setting pager failed");
		return -3;
	}

	/* get the thread running on the right cpu */
	affinity(_location);

	/* assign priority */
	if (!L4_Set_Priority(thread,
	                     Cpu_session::scale_priority(DEFAULT_PRIORITY, _priority)))
		warning("could not set thread prioritry to default");

	/* send start message */
	L4_Msg_t msg;
	L4_Clear(&msg);
	L4_Append(&msg, (L4_Word_t)ip);
	L4_Append(&msg, (L4_Word_t)sp);
	L4_Load(&msg);

	L4_MsgTag_t tag = L4_Send(thread);

	if (L4_IpcFailed(tag)) {
		error("starting thread failed. (IPC error)");
		return -4;
	}

	return 0;
}


void Platform_thread::pause()
{
	warning(__func__, " not implemented");
}


void Platform_thread::resume()
{
	warning(__func__, " not implemented");
}


void Platform_thread::bind(int thread_id, L4_ThreadId_t l4_thread_id,
                           Platform_pd *pd)
{
  _thread_id    = thread_id;
  _l4_thread_id = l4_thread_id;
  _platform_pd  = pd;
}


void Platform_thread::unbind()
{
	L4_Word_t res = L4_ThreadControl(_l4_thread_id, L4_nilthread,
	                                 L4_nilthread, L4_nilthread, (void *)-1);

	if (res != 1)
		error("deleting thread ", Formatted_tid(_l4_thread_id), " failed");

	_thread_id    = THREAD_INVALID;
	_l4_thread_id = L4_nilthread;
	_platform_pd  = 0;
}


void Platform_thread::state(Thread_state)
{
	warning(__func__, " not implemented");
	throw Cpu_thread::State_access_failed();
}


Thread_state Platform_thread::state()
{
	Thread_state s;

	L4_Word_t     dummy;
	L4_ThreadId_t dummy_tid;
	L4_Word_t ip, sp;

	enum {
		DELIVER = 1 << 9,
	};

	L4_ExchangeRegisters(_l4_thread_id,
	                     DELIVER,
	                     0, 0, 0, 0, L4_nilthread,
	                     &dummy, &sp, &ip, &dummy, &dummy,
	                     &dummy_tid);
	s.ip = ip;
	s.sp = sp;
	return s;
}


void Platform_thread::cancel_blocking()
{
	L4_Word_t     dummy;
	L4_ThreadId_t dummy_tid;

	/*
	 * XXX: This implementation is not safe because it only cancels
	 *      a currently executed blocking operation but it has no
	 *      effect when the thread is executing user code and going
	 *      to block soon. To solve this issue, we would need signalling
	 *      semantics, which means that we flag the thread to being
	 *      canceled the next time it enters the kernel.
	 */

	/* control flags for 'L4_ExchangeRegisters' */
	enum {
		CANCEL_SEND         = 1 << 2,
		CANCEL_RECV         = 1 << 1,
		CANCEL_IPC          = CANCEL_SEND | CANCEL_RECV,
		USER_DEFINED_HANDLE = 1 << 6,
		RESUME              = 1 << 8,
	};

	/* reset value for the thread's user-defined handle */
	enum { USER_DEFINED_HANDLE_ZERO = 0 };

	L4_ExchangeRegisters(_l4_thread_id,
	                     CANCEL_IPC | RESUME | USER_DEFINED_HANDLE,
	                     0, 0, 0, USER_DEFINED_HANDLE_ZERO, L4_nilthread,
	                     &dummy, &dummy, &dummy, &dummy, &dummy,
	                     &dummy_tid);
}


Weak_ptr<Address_space> Platform_thread::address_space()
{
	return _platform_pd->Address_space::weak_ptr();
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned prio,
                                 Affinity::Location, addr_t, int id)
: _thread_id(id), _l4_thread_id(L4_nilthread), _priority(prio), _pager(0)
{
	strncpy(_name, name, sizeof(_name));
}


Platform_thread::~Platform_thread()
{
	/*
	 * We inform our protection domain about thread destruction, which will end up in
	 * Thread::unbind()
	 */
	if (_platform_pd)
		_platform_pd->unbind_thread(this);
}
