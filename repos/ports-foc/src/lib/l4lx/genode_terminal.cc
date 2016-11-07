/*
 * \brief  Genode C API terminal functions of the L4Linux support library
 * \author Stefan Kalkowski
 * \date   2011-09-16
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/thread.h>
#include <terminal_session/connection.h>
#include <foc/capability_space.h>

#include <linux.h>
#include <vcpu.h>

namespace Fiasco {
#include <genode/net.h>
#include <l4/sys/irq.h>
#include <l4/sys/kdebug.h>
}

static Terminal::Connection *terminal() {
	static bool initialized = false;
	static Terminal::Connection *t = 0;

	if (!initialized) {
		try {
			static Terminal::Connection terminal;
			t = &terminal;
		} catch(...) { }
		initialized = true;
	}
	return t;
}


namespace {
	class Signal_thread : public Genode::Thread_deprecated<8192>
	{
		private:

			Fiasco::l4_cap_idx_t _cap;

		protected:

			void entry()
			{
				using namespace Fiasco;
				using namespace Genode;

				Signal_receiver           receiver;
				Signal_context            rx;
				Signal_context_capability cap(receiver.manage(&rx));
				terminal()->connected_sigh(cap);
				terminal()->read_avail_sigh(cap);

				while (true) {
					receiver.wait_for_signal();
					if (l4_error(l4_irq_trigger(_cap)) != -1)
						PWRN("IRQ terminal trigger failed\n");
				}
			}

		public:

			Signal_thread(Fiasco::l4_cap_idx_t cap)
			: Genode::Thread_deprecated<8192>("terminal-signal-thread"), _cap(cap) { start(); }
	};
}


static Signal_thread *signal_thread = 0;


using namespace Fiasco;

extern "C" {

	unsigned genode_terminal_readchar(unsigned idx, char *buf, unsigned long sz) {
		if (!terminal()->avail())
			return 0;
		return terminal()->read(buf, sz);
	}


	void genode_terminal_writechar(unsigned idx, const char *buf, unsigned long sz) {
		terminal()->write(buf, sz);
	}


	l4_cap_idx_t genode_terminal_irq(unsigned idx) {

		Genode::Foc_native_cpu_client
			native_cpu(L4lx::cpu_connection()->native_cpu());

		static Genode::Native_capability cap = native_cpu.alloc_irq();
		l4_cap_idx_t const kcap = Genode::Capability_space::kcap(cap);
		if (!signal_thread)
			signal_thread = new (Genode::env()->heap()) Signal_thread(kcap);
		return kcap;
	}


	unsigned genode_terminal_count(void) {
		return terminal() ? 1 : 0; }


	void genode_terminal_stop(unsigned idx) {
		destroy(Genode::env()->heap(), signal_thread);
	}
}
