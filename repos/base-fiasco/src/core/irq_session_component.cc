/*
 * \brief  Core implementation of IRQ sessions
 * \author Christian Helmuth
 * \date   2007-09-13
 *
 * FIXME ram quota missing
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_proxy.h>
#include <irq_root.h>
#include <util.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/types.h>
}

namespace Genode {
       typedef Irq_proxy<Thread<1024 * sizeof(addr_t)> > Irq_proxy_base;
       class Irq_proxy_component;
}


using namespace Genode;


/**
 * Platform-specific proxy code
 */

class Genode::Irq_proxy_component : public Irq_proxy_base
{
	protected:

		bool _associate()
		{
			using namespace Fiasco;

			int err;
			l4_threadid_t irq_tid;
			l4_umword_t dw0, dw1;
			l4_msgdope_t result;

			l4_make_taskid_from_irq(_irq_number, &irq_tid);

			/* boost thread to IRQ priority */
			enum { IRQ_PRIORITY = 0xC0 };

			l4_sched_param_t param = {sp:{prio:IRQ_PRIORITY, small:0, state:0,
			                              time_exp:0, time_man:0}};
			l4_threadid_t    ext_preempter = L4_INVALID_ID;
			l4_threadid_t    partner       = L4_INVALID_ID;
			l4_sched_param_t old_param;
			l4_thread_schedule(l4_myself(), param, &ext_preempter, &partner,
			                   &old_param);

			err = l4_ipc_receive(irq_tid,
			                     L4_IPC_SHORT_MSG, &dw0, &dw1,
			                     L4_IPC_BOTH_TIMEOUT_0, &result);

			if (err != L4_IPC_RETIMEOUT) PERR("IRQ association failed");

			return (err == L4_IPC_RETIMEOUT);
		}

		void _wait_for_irq()
		{
			using namespace Fiasco;

			l4_threadid_t irq_tid;
			l4_umword_t dw0=0, dw1=0;
			l4_msgdope_t result;

			l4_make_taskid_from_irq(_irq_number, &irq_tid);

			do {
				l4_ipc_call(irq_tid,
				            L4_IPC_SHORT_MSG, 0, 0,
				            L4_IPC_SHORT_MSG, &dw0, &dw1,
				            L4_IPC_NEVER, &result);

				if (L4_IPC_IS_ERROR(result))
					PERR("Ipc error %lx", L4_IPC_ERROR(result));
			} while (L4_IPC_IS_ERROR(result));
		}

		void _ack_irq() { }

	public:

		Irq_proxy_component(long irq_number)
		:
			Irq_proxy(irq_number)
		{
			_start();
		}
};


/***************************
 ** IRQ session component **
 ***************************/


void Irq_session_component::ack_irq()
{
	if (!_proxy) {
		PERR("Expected to find IRQ proxy for IRQ %02x", _irq_number);
		return;
	}

	_proxy->ack_irq();
}


Irq_session_component::Irq_session_component(Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc)
{
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1) {
		PERR("invalid IRQ number requested");
		throw Root::Unavailable();
	}

	long msi = Arg_string::find_arg(args, "device_config_phys").long_value(0);
	if (msi)
		throw Root::Unavailable();

	/* check if IRQ thread was started before */
	_proxy = Irq_proxy_component::get_irq_proxy<Irq_proxy_component>(irq_number, irq_alloc);
	if (!_proxy) {
		PERR("unavailable IRQ %lx requested", irq_number);
		throw Root::Unavailable();
	}

	_irq_number = irq_number;
}


Irq_session_component::~Irq_session_component()
{
	if (!_proxy) return;

	if (_irq_sigh.valid())
		_proxy->remove_sharer(&_irq_sigh);
}


void Irq_session_component::sigh(Genode::Signal_context_capability sigh)
{
	if (!_proxy) {
		PERR("signal handler got not registered - irq thread unavailable");
		return;
	}

	Genode::Signal_context_capability old = _irq_sigh;

	if (old.valid() && !sigh.valid())
		_proxy->remove_sharer(&_irq_sigh);

	_irq_sigh = sigh;

	if (!old.valid() && sigh.valid())
		_proxy->add_sharer(&_irq_sigh);
}


Genode::Irq_session::Info Irq_session_component::info()
{
	/* no MSI support */
	return { .type = Genode::Irq_session::Info::Type::INVALID };
}
