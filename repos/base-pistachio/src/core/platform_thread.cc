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

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>
#include <base/internal/pistachio.h>

/* core includes */
#include <platform_pd.h>
#include <platform_thread.h>
#include <kip.h>
#include <print_l4_thread_id.h>

using namespace Core;
using namespace Pistachio;


void Platform_thread::affinity(Affinity::Location location)
{
	unsigned const cpu_no = location.xpos();

	if (cpu_no >= L4_NumProcessors(get_kip())) {
		error("invalid processor number");
		return;
	}

	if (native_thread_id() != L4_nilthread) {
		if (L4_Set_ProcessorNo(native_thread_id(), cpu_no) == 0)
			error("could not set processor number");
		else
			_location = location;
	}
}


Affinity::Location Platform_thread::affinity() const
{
	return _location;
}


void Platform_thread::start(void *ip, void *sp)
{
	if (_id.failed())
		return;

	L4_ThreadId_t thread = native_thread_id();
	L4_ThreadId_t pager  = _pager
	                     ? Capability_space::ipc_cap_data(_pager->cap()).dst
	                     : L4_nilthread;

	/* XXX should always be the root task */
	L4_ThreadId_t preempter = L4_Myself();

	L4_Word_t const utcb_location = _id.convert<L4_Word_t>(
		[&] (Platform_pd::Thread_id id) { return _pd._utcb_location(id.value); },
		[&] (Platform_pd::Alloc_thread_id_error) { return 0UL; });

	int ret = L4_ThreadControl(thread, _pd._l4_task_id,
	                           preempter, L4_Myself(), (void *)utcb_location);

	if (ret != 1) {
		error(__func__, ": L4_ThreadControl returned ", Hex(L4_ErrorCode()));
		return;
	}

	/* set real pager */
	ret = L4_ThreadControl(thread, _pd._l4_task_id,
	                       L4_nilthread, pager, (void *)-1);

	if (ret != 1) {
		error(__func__, ": L4_ThreadControl returned ", Hex(L4_ErrorCode()));
		error("setting pager failed");
		return;
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
		return;
	}
}


Thread_state Platform_thread::state()
{
	Thread_state s { };

	L4_Word_t     dummy;
	L4_ThreadId_t dummy_tid;
	L4_Word_t ip, sp;

	enum {
		DELIVER = 1 << 9,
	};

	L4_ExchangeRegisters(native_thread_id(),
	                     DELIVER,
	                     0, 0, 0, 0, L4_nilthread,
	                     &dummy, &sp, &ip, &dummy, &dummy,
	                     &dummy_tid);
	s.cpu.ip = ip;
	s.cpu.sp = sp;
	s.state  = Thread_state::State::VALID;
	return s;
}


Platform_thread::~Platform_thread()
{
	_id.with_result(
		[&] (Platform_pd::Thread_id id) {

			L4_Word_t res = L4_ThreadControl(native_thread_id(), L4_nilthread,
			                                 L4_nilthread, L4_nilthread, (void *)-1);
			if (res != 1)
				error("deleting thread ", Formatted_tid(native_thread_id()), " failed");

			_pd.free_thread_id(id);
		},
		[&] (Platform_pd::Alloc_thread_id_error) { }
	);
}
