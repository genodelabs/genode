/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* VirtualBox includes */
#include <VBox/vmm/hwacc_vmx.h>

#include "vmm_memory.h"

/* Genode's VirtualBox includes */
#include "vcpu.h"
#include "vmx.h"

extern "C" int MMIO2_MAPPED_SYNC(PVM pVM, RTGCPHYS GCPhys, size_t cbWrite);

class Vcpu_handler_vmx : public Vcpu_handler
{
	private:

		template <unsigned X>
		__attribute__((noreturn)) void _vmx_ept()
		{
			using namespace Nova;
			using namespace Genode;

			Thread_base *myself = Thread_base::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			_exc_memory<X>(myself, utcb, utcb->qual[0] & 0x38,
			               utcb->qual[1] & ~((1UL << 12) - 1));
		}

		__attribute__((noreturn)) void _vmx_default() { _default_handler(); }

		__attribute__((noreturn)) void _vmx_startup()
		{
			using namespace Nova;

			Genode::Thread_base *myself = Genode::Thread_base::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			/* avoid as many as possible VM exits */
			next_utcb.mtd = Nova::Mtd::CTRL;
			next_utcb.ctrl[0] = 0;
			next_utcb.ctrl[1] = 0;

			void *exit_status = _start_routine(_arg);
			pthread_exit(exit_status);

			Nova::reply(nullptr);
		}

		__attribute__((noreturn)) void _vmx_triple()
		{
			Genode::Thread_base *myself = Genode::Thread_base::myself();
			using namespace Nova;

			Vmm::printf("triple fault - dead\n");

			_default_handler();
		}

		__attribute__((noreturn)) void _vmx_irqwin()
		{
			_irq_window(VMX_EXIT_IRQ_WINDOW);
		}

	public:

		Vcpu_handler_vmx(size_t stack_size, const pthread_attr_t *attr,
	                     void *(*start_routine) (void *), void *arg)
		: Vcpu_handler(stack_size, attr, start_routine, arg) 
		{
			using namespace Nova;

			typedef Vcpu_handler_vmx This;

			Genode::addr_t const exc_base = vcpu().exc_base();

			register_handler<VMX_EXIT_TRIPLE_FAULT, This,
				&This::_vmx_triple> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_INIT_SIGNAL, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_IRQ_WINDOW, This,
				&This::_vmx_irqwin> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_CPUID, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_HLT, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_RDTSC, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_VMCALL, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_PORT_IO, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_RDMSR, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_WRMSR, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_ERR_INVALID_GUEST_STATE, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_PAUSE, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_EPT_VIOLATION, This,
				&This::_vmx_ept<VMX_EXIT_EPT_VIOLATION>> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VCPU_STARTUP, This,
				&This::_vmx_startup> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<RECALL, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);

			start();
		}

		bool hw_save_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return vmx_save_state(utcb, pVM, pVCpu);
		}

		bool hw_load_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return vmx_load_state(utcb, pVM, pVCpu);
		}
};
