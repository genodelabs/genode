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
#include <base/internal/pistachio.h>

/* core includes */
#include <platform_pd.h>
#include <platform_thread.h>
#include <kip.h>
#include <print_l4_thread_id.h>

using namespace Genode;
using namespace Pistachio;


void Platform_thread::affinity(Affinity::Location location)
{
	unsigned const cpu_no = location.xpos();

	if (cpu_no >= L4_NumProcessors(get_kip())) {
		error("invalid processor number");
		return;
	}

	if (_l4_thread_id != L4_nilthread) {
		if (L4_Set_ProcessorNo(_l4_thread_id, cpu_no) == 0)
			error("could not set processor number");
		else
			_location = location;
	}
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
                           Platform_pd &pd)
{
	_thread_id    = thread_id;
	_l4_thread_id = l4_thread_id;
	_platform_pd  = &pd;
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


Platform_thread::~Platform_thread()
{
	/*
	 * We inform our protection domain about thread destruction, which will end up in
	 * Thread::unbind()
	 */
	if (_platform_pd)
		_platform_pd->unbind_thread(*this);
}
