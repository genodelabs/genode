/*
 * \brief  Fiasco.OC-specific signal-source client interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-02-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <signal_source/client.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/capability_data.h>
#include <base/internal/native_thread.h>

/* Fiasco.OC includes */
#include <foc_native_cpu/client.h>
#include <foc/syscall.h>

using namespace Genode;


Signal_source_client::Signal_source_client(Cpu_session &cpu, Capability<Signal_source> cap)
:
	Rpc_client<Foc_signal_source>(static_cap_cast<Foc_signal_source>(cap)),

	/* request mapping of semaphore capability selector */
	_sem(call<Rpc_request_semaphore>())
{
	using namespace Foc;

	Foc_native_cpu_client cpu_client(cpu.native_cpu());
	Native_capability thread_cap = cpu_client.native_cap(Thread::myself()->cap());
	l4_msgtag_t tag = l4_rcv_ep_bind_thread(_sem.data()->kcap(), thread_cap.data()->kcap(), 0);
	if (l4_error(tag))
		Genode::raw("l4_rcv_ep_bind_thread failed with ", l4_error(tag));
}


Signal_source_client::~Signal_source_client()
{
	Foc::l4_irq_detach(_sem.data()->kcap());
}


__attribute__((optimize("-fno-omit-frame-pointer")))
__attribute__((noinline))
Signal_source_client::Signal Signal_source_client::wait_for_signal()
{
	using namespace Foc;

	Signal signal;
	do {

		/* block on semaphore until signal context was submitted */
		l4_irq_receive(_sem.data()->kcap(), L4_IPC_NEVER);

		/*
		 * The following request will return immediately and either
		 * return a valid or a null signal. The latter may happen in
		 * the case a submitted signal context was destroyed (by the
		 * submitter) before we have a chance to raise our request.
		 */
		signal = call<Rpc_wait_for_signal>();

	} while (!signal.imprint());

	return signal;
}
