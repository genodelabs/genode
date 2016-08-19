/*
 * \brief  Test for Genode's VMM utilities
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/sleep.h>

/* VMM utility includes */
#include <vmm/vcpu_thread.h>
#include <vmm/vcpu_dispatcher.h>
#include <vmm/printf.h>

using Genode::Cap_connection;
using Genode::sleep_forever;


class Vcpu_dispatcher : public Vmm::Vcpu_dispatcher<Genode::Thread>
{
	private:

		typedef Vcpu_dispatcher This;

		Vmm::Vcpu_same_pd _vcpu_thread;

		/**
		 * Shortcut for calling 'Vmm::Vcpu_dispatcher::register_handler'
		 * with 'Vcpu_dispatcher' as template argument
		 */
		template <unsigned EV, void (This::*FUNC)()>
		void _register_handler(Genode::addr_t exc_base, Nova::Mtd mtd)
		{
			register_handler<EV, This, FUNC>(exc_base, mtd);
		}

		enum { STACK_SIZE = 1024*sizeof(long) };


		/***********************************
		 ** Virtualization event handlers **
		 ***********************************/

		void _svm_startup()
		{
			Vmm::log("_svm_startup called");
		}

	public:

		enum Type { SVM, VTX };

		Vcpu_dispatcher(Cap_connection &cap, Type type)
		:
			Vmm::Vcpu_dispatcher<Genode::Thread>(STACK_SIZE, cap, Genode::env()->cpu_session(), Genode::Affinity::Location()),
			_vcpu_thread(STACK_SIZE, Genode::env()->cpu_session(), Genode::Affinity::Location())
		{
			using namespace Nova;

			/* shortcuts for common message-transfer descriptors */
			Mtd const mtd_all(Mtd::ALL);
			Mtd const mtd_cpuid(Mtd::EIP | Mtd::ACDB | Mtd::IRQ);
			Mtd const mtd_irq(Mtd::IRQ);

			Genode::addr_t exc_base = _vcpu_thread.exc_base();

			/* register virtualization event handlers */
			if (type == SVM) {
				_register_handler<0xfe, &This::_svm_startup>
					(exc_base, mtd_all);
			}

			/* start virtual CPU */
			_vcpu_thread.start(sel_sm_ec() + 1);
		}
};


int main(int argc, char **argv)
{
	Genode::log("--- VBox started ---");

	static Cap_connection cap;
	static Vcpu_dispatcher vcpu_dispatcher(cap, Vcpu_dispatcher::SVM);

	Genode::log("going to sleep forever...");
	sleep_forever();
	return 0;
}
