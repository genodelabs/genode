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
#include <base/env.h>
#include <base/log.h>

/* base-internal includes */
#include <base/internal/capability_data.h>
#include <base/internal/native_thread.h>

/* Fiasco includes */
#include <foc_native_cpu/client.h>

namespace Fiasco {
#include <l4/sys/irq.h>
}

using namespace Genode;


Signal_source_client::Signal_source_client(Capability<Signal_source> cap)
:
	Rpc_client<Foc_signal_source>(static_cap_cast<Foc_signal_source>(cap))
{
	using namespace Fiasco;

	/* request mapping of semaphore capability selector */
	_sem = call<Rpc_request_semaphore>();

	Foc_native_cpu_client cpu_client(env_deprecated()->cpu_session()->native_cpu());
	Native_capability thread_cap = cpu_client.native_cap(Thread::myself()->cap());
	l4_msgtag_t tag = l4_irq_attach(_sem.data()->kcap(), 0, thread_cap.data()->kcap());
	if (l4_error(tag))
		Genode::raw("l4_irq_attach failed with ", l4_error(tag));
}


Signal_source_client::~Signal_source_client()
{
	Fiasco::l4_irq_detach(_sem.data()->kcap());
}


__attribute__((optimize("-fno-omit-frame-pointer")))
__attribute__((noinline))
Signal_source_client::Signal Signal_source_client::wait_for_signal()
{
	using namespace Fiasco;

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
