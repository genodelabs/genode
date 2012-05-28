/*
 * \brief   Pistachio thread facility
 * \author  Julian Stecklina
 * \date    2008-03-19
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/string.h>
#include <pistachio/thread_helper.h>
#include <pistachio/kip.h>

/* core includes */
#include <platform_pd.h>
#include <platform_thread.h>

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

#define PT_DBG(args...) if (verbose) PDBG(args); else {}


void Platform_thread::set_cpu(unsigned int cpu_no)
{
	if (cpu_no >= L4_NumProcessors(get_kip())) {
		PERR("Invalid processor number.");
		return;
	}

	if (L4_Set_ProcessorNo(_l4_thread_id, cpu_no) == 0)
		PERR("Error setting processor number.");
}


int Platform_thread::start(void *ip, void *sp, unsigned int cpu_no)
{
	L4_ThreadId_t thread = _l4_thread_id;
	L4_ThreadId_t pager  = _pager ? _pager->cap().dst() : L4_nilthread;

	/* XXX should always be the root task */
	L4_ThreadId_t preempter = L4_Myself();

	PT_DBG("Trying to Platform_thread::start the thread '%s'.", _name);

	if (verbose2)
		printf("thread '%s' has id 0x%08lx (task = 0x%x, thread = 0x%x)\n",
		       _name, thread.raw, _platform_pd->pd_id(), _thread_id);

	if (_thread_id == THREAD_INVALID) {
		PERR("Trying to start a thread with invalid ID.");
		return -1;
	}

	L4_Word_t utcb_location = _platform_pd->_utcb_location(_thread_id);

	PT_DBG("New thread's utcb at %08lx.", utcb_location);
	PT_DBG("Attaching thread to address space 0x%08lx.",
	       _platform_pd->_l4_task_id.raw);

	PT_DBG("sp = %p, ip =  %p", sp, ip);
	int ret = L4_ThreadControl(thread, _platform_pd->_l4_task_id,
	                           preempter, L4_Myself(), (void *)utcb_location);

	PT_DBG("L4_ThreadControl() = %d", ret);
	if (ret != 1) {
		PERR("Error code = 0x%08lx", L4_ErrorCode());
		PERR("L4_ThreadControl failed.");
		return -2;
	}

	/* set real pager */
	ret = L4_ThreadControl(thread, _platform_pd->_l4_task_id,
	                       L4_nilthread, pager, (void *)-1);

	if (ret != 1) {
		PERR("Error code = 0x%08lx", L4_ErrorCode());
		PERR("Setting pager failed.");
		return -3;
	}

	/* get the thread running on the right cpu */
	set_cpu(cpu_no);

	/* assign priority */
	if (!L4_Set_Priority(thread,
	                     Cpu_session::scale_priority(DEFAULT_PRIORITY, _priority)))
		PWRN("Could not set thread prioritry to default");

	/* send start message */
	L4_Msg_t msg;
	L4_Clear(&msg);
	L4_Append(&msg, (L4_Word_t)ip);
	L4_Append(&msg, (L4_Word_t)sp);
	L4_Load(&msg);

	L4_MsgTag_t tag = L4_Send(thread);

	if (L4_IpcFailed(tag)) {
		PERR("Starting thread failed. (IPC error)");
		return -4;
	}

	PT_DBG("Done starting thread.");

	return 0;
}


void Platform_thread::pause()
{
	PDBG("not implemented");
}


void Platform_thread::resume()
{
	PDBG("not implemented");
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
	PT_DBG("Killing thread 0x%08lx.", _l4_thread_id.raw);

	L4_Word_t res = L4_ThreadControl(_l4_thread_id, L4_nilthread,
	                                 L4_nilthread, L4_nilthread, (void *)-1);

	if (res != 1)
		PERR("Deleting thread 0x%08lx failed. Continuing...", _l4_thread_id.raw);

	_thread_id    = THREAD_INVALID;
	_l4_thread_id = L4_nilthread;
	_platform_pd  = 0;
}


int Platform_thread::state(Thread_state *state_dst)
{
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
	state_dst->ip = ip;
	state_dst->sp = sp;
	return 0;
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


Platform_thread::Platform_thread(const char *name, unsigned prio, addr_t, int id)
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
