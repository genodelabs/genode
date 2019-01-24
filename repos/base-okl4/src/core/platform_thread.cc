/*
 * \brief   OKL4 thread facility
 * \author  Julian Stecklina
 * \author  Norman Feske
 * \author  Stefan Kalkowski
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
#include <util/misc_math.h>

/* core includes */
#include <platform.h>
#include <platform_pd.h>
#include <platform_thread.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>
#include <base/internal/okl4.h>

using namespace Genode;
using namespace Okl4;


int Platform_thread::start(void *ip, void *sp, unsigned)
{
	if (!_platform_pd) {
		warning("thread ", _thread_id, " is not bound to a PD");
		return -1;
	}

	/* activate local thread by assigning a UTCB address and thread ID */
	int           space_no           = _platform_pd->pd_id();
	L4_ThreadId_t new_thread_id      = _platform_pd->make_l4_id(space_no,
	                                                            _thread_id);
	L4_SpaceId_t  space_id           = L4_SpaceId(space_no);
	L4_ThreadId_t scheduler          = L4_rootserver;

	L4_ThreadId_t pager  = _pager
	                     ? Capability_space::ipc_cap_data(_pager->cap()).dst
	                     : L4_nilthread;

	L4_ThreadId_t exception_handler  = pager;
	L4_Word_t     resources          = 0;
	L4_Word_t     utcb_size_per_task = L4_GetUtcbSize()*(1 << Thread_id_bits::THREAD);
	L4_Word_t     utcb_location      = platform_specific().utcb_base()
	                                 + _platform_pd->pd_id()*utcb_size_per_task
	                                 + _thread_id*L4_GetUtcbSize();
	/*
	 * On some ARM architectures, UTCBs are allocated by the kernel.
	 * In this case, we need to specify -1 as UTCB location to prevent
	 * the thread creation to fail with an 'L4_ErrUtcbArea' error.
	 */
#ifdef NO_UTCB_RELOCATE
	utcb_location = ~0;
#endif

	/*
	 * If a pager for the PD was set before, we will use it as the pager
	 * of this thread.
	 *
	 * Note: This is used by OKLinux only
	 */
	if(_platform_pd && _platform_pd->space_pager()) {
		pager = _platform_pd->space_pager()->_l4_thread_id;
		exception_handler = pager;
	}

	int ret = L4_ThreadControl(new_thread_id,
	                           space_id,
	                           scheduler, pager, exception_handler,
	                           resources, (void *)utcb_location);
	if (ret != 1) {
		error("L4_ThreadControl returned ", ret, ", error=", ret, L4_ErrorCode());
		return -1;
	}

	/* make the symbolic thread name known to the kernel debugger */
	L4_KDB_SetThreadName(new_thread_id, _name);

	/* let the new thread know its global thread id */
	L4_Set_UserDefinedHandleOf(new_thread_id, new_thread_id.raw);

	/*
	 * Don't start if ip and sp are set invalid.
	 *
	 * Note: This quirk is only used by OKLinux
	 */
	if((L4_Word_t)sp != 0xffffffff || (L4_Word_t)ip != 0xffffffff)
		L4_Start_SpIp(new_thread_id, (L4_Word_t)sp, (L4_Word_t)ip);

	/* assign priority */
	if (!L4_Set_Priority(new_thread_id,
	                     Cpu_session::scale_priority(DEFAULT_PRIORITY, _priority)))
		warning("could not set thread prioritry to default");

	set_l4_thread_id(new_thread_id);
	return 0;
}


void Platform_thread::pause()
{
	L4_SuspendThread(_l4_thread_id);
}


void Platform_thread::resume()
{
	L4_UnsuspendThread(_l4_thread_id);
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
	L4_Word_t res = L4_ThreadControl(_l4_thread_id, L4_nilspace,
	                                 L4_nilthread, L4_nilthread, L4_nilthread, ~0, 0);

	if (res != 1)
		error("deleting thread ", Hex(_l4_thread_id.raw), " failed");

	_thread_id    = THREAD_INVALID;
	_l4_thread_id = L4_nilthread;
	_platform_pd  = nullptr;
}


void Platform_thread::cancel_blocking()
{
	L4_Word_t     dummy;
	L4_ThreadId_t dummy_tid;

	/*
	 * For more details, please refer to the corresponding implementation in
	 * the 'base-pistachio' repository.
	 */

	/* reset value for the thread's user-defined handle */
	enum { USER_DEFINED_HANDLE_ZERO = 0 };

	L4_ExchangeRegisters(_l4_thread_id,
	                     L4_ExReg_Resume | L4_ExReg_AbortOperation | L4_ExReg_user,
	                     0, 0, 0, USER_DEFINED_HANDLE_ZERO, L4_nilthread,
	                     &dummy, &dummy, &dummy, &dummy, &dummy,
	                     &dummy_tid);
}


unsigned long Platform_thread::pager_object_badge() const
{
	return native_thread_id().raw;
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned prio,
                                 Affinity::Location, addr_t)
:
	_l4_thread_id(L4_nilthread), _platform_pd(0),
	_priority(prio), _pager(0)
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
		_platform_pd->unbind_thread(*this);
}
