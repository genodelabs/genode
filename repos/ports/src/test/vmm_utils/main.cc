/*
 * \brief  Test for Genode's VMM utilities
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/component.h>

/* VMM utility includes */
#include <vmm/vcpu_thread.h>
#include <vmm/vcpu_dispatcher.h>
#include <vmm/printf.h>


template <typename T>
class Vcpu_dispatcher : public Vmm::Vcpu_dispatcher<Genode::Thread>
{
	private:

		typedef Vcpu_dispatcher This;

		T _vcpu_thread;

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

		void _vcpu_startup()
		{
			Vmm::log(name(), " ", __func__, " called");
		}

	public:

		enum Type { SVM, VTX };

		Vcpu_dispatcher(Genode::Env &env, Type type, char const * name,
		                Genode::Capability<Genode::Pd_session> pd_cap)
		:
			Vmm::Vcpu_dispatcher<Genode::Thread>(env, STACK_SIZE, &env.cpu(),
			                                     Genode::Affinity::Location(),
			                                     name),
			_vcpu_thread(&env.cpu(), Genode::Affinity::Location(), pd_cap,
			             STACK_SIZE)
		{
			using namespace Nova;

			/* shortcuts for common message-transfer descriptors */
			Mtd const mtd_all(Mtd::ALL);
			Mtd const mtd_cpuid(Mtd::EIP | Mtd::ACDB | Mtd::IRQ);
			Mtd const mtd_irq(Mtd::IRQ);

			Genode::addr_t exc_base = _vcpu_thread.exc_base();

			/* register virtualization event handlers */
			if (type == SVM) {
				_register_handler<0xfe, &This::_vcpu_startup>
					(exc_base, mtd_all);
			}

			/* start virtual CPU */
			_vcpu_thread.start(sel_sm_ec() + 1);
		}
};


void Component::construct(Genode::Env &env)
{
	typedef Vcpu_dispatcher<Vmm::Vcpu_same_pd> Vcpu_s;

	static Vcpu_s vcpu_s_1(env, Vcpu_s::SVM, "vcpu_s_1", env.pd_session_cap());
	static Vcpu_s vcpu_s_2(env, Vcpu_s::SVM, "vcpu_s_2", env.pd_session_cap());

	typedef Vcpu_dispatcher<Vmm::Vcpu_other_pd> Vcpu_o;

	static Genode::Pd_connection remote_pd(env, "VM");
	static Vcpu_o vcpu_o_1(env, Vcpu_o::SVM, "vcpu_o_1", remote_pd);
	static Vcpu_o vcpu_o_2(env, Vcpu_o::SVM, "vcpu_o_2", remote_pd);
}
