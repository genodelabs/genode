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

#ifndef _VIRTUALBOX__SPEC__NOVA__VCPU_VMX_H_
#define _VIRTUALBOX__SPEC__NOVA__VCPU_VMX_H_

/* libc includes */
#include <stdlib.h>

/* VirtualBox includes */
#include <VBox/vmm/hm_vmx.h>

#include "vmm_memory.h"

/* Genode's VirtualBox includes */
#include "vcpu.h"
#include "vmx.h"


class Vcpu_handler_vmx : public Vcpu_handler
{
	private:

		template <unsigned X>
		__attribute__((noreturn)) void _vmx_ept()
		{
			using namespace Nova;
			using namespace Genode;

			Thread *myself = Thread::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			_exc_memory<X>(myself, utcb, utcb->qual[0] & 0x38,
			               utcb->qual[1] & ~((1UL << 12) - 1));
		}

		__attribute__((noreturn)) void _vmx_default() { _default_handler(); }

		__attribute__((noreturn)) void _vmx_startup()
		{
			using namespace Nova;

			Genode::Thread *myself = Genode::Thread::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			/* configure VM exits to get */
			next_utcb.mtd = Nova::Mtd::CTRL;
			/* from src/VBox/VMM/VMMR0/HWVMXR0.cpp of virtualbox sources  */
			next_utcb.ctrl[0] = VMX_VMCS_CTRL_PROC_EXEC_HLT_EXIT |
			                    VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT |
			                    VMX_VMCS_CTRL_PROC_EXEC_UNCOND_IO_EXIT |
/* XXX commented out because TinyCore Linux won't run as guest otherwise
			                    VMX_VMCS_CTRL_PROC_EXEC_MONITOR_EXIT |
			                    VMX_VMCS_CTRL_PROC_EXEC_MWAIT_EXIT |
*/
/*			                    VMX_VMCS_CTRL_PROC_EXEC_CR8_LOAD_EXIT |
			                    VMX_VMCS_CTRL_PROC_EXEC_CR8_STORE_EXIT |*/
			                    VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW |
			                    VMX_VMCS_CTRL_PROC_EXEC_RDPMC_EXIT;
/*			                    VMX_VMCS_CTRL_PROC_EXEC_PAUSE_EXIT | */
			/*
			 * Disable trapping RDTSC for now as it creates a huge load with
			 * VM guests that execute it frequently.
			 */
			                    // VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT;

			next_utcb.ctrl[1] = VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC |
			                    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT |
			                    VMX_VMCS_CTRL_PROC_EXEC2_UNRESTRICTED_GUEST |
			                    VMX_VMCS_CTRL_PROC_EXEC2_VPID |
/*			                    VMX_VMCS_CTRL_PROC_EXEC2_X2APIC | */
			                    VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP |
			                    VMX_VMCS_CTRL_PROC_EXEC2_EPT;

			void *exit_status = _start_routine(_arg);
			pthread_exit(exit_status);

			Nova::reply(nullptr);
		}

		__attribute__((noreturn)) void _vmx_triple()
		{
			Vmm::error("triple fault - dead");
			exit(-1);
		}

		__attribute__((noreturn)) void _vmx_irqwin() { _irq_window(); }

		__attribute__((noreturn)) void _vmx_recall()
		{
			Vcpu_handler::_recall_handler();
		}

		__attribute__((noreturn)) void _vmx_invalid()
		{
			Genode::Thread *myself = Genode::Thread::myself();
			Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

			unsigned const dubious = utcb->inj_info |
			                         utcb->intr_state | utcb->actv_state;
			if (dubious)
				Vmm::warning(__func__, " - dubious -"
				             " inj_info=", Genode::Hex(utcb->inj_info),
				             " inj_error=", Genode::Hex(utcb->inj_error),
				             " intr_state=", Genode::Hex(utcb->intr_state),
				             " actv_state=", Genode::Hex(utcb->actv_state));

			Vmm::error("invalid guest state - dead");
			exit(-1);
		}

		/*
		 * This VM exit is in part handled by the NOVA kernel (writing the CR
		 * register) and in part by VirtualBox (updating the PDPTE registers,
		 * which requires access to the guest physical memory).
		 * Intel manual sections 4.4.1 of Vol. 3A and 26.3.2.4 of Vol. 3C
		 * indicate the conditions when the PDPTE registers need to get
		 * updated.
		 */
		__attribute__((noreturn)) void _vmx_mov_crx()
		{
			unsigned long value;
			void *stack_reply = reinterpret_cast<void *>(&value - 1);

			Genode::Thread *myself = Genode::Thread::myself();
			Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

			unsigned int cr = utcb->qual[0] & 0xf;

			if (cr == 8)
				_default_handler();

			Genode::uint64_t *pdpte = (Genode::uint64_t*)
				guest_memory()->lookup(utcb->cr3, sizeof(utcb->pdpte));

			Assert(pdpte != 0);

			utcb->pdpte[0] = pdpte[0];
			utcb->pdpte[1] = pdpte[1];
			utcb->pdpte[2] = pdpte[2];
			utcb->pdpte[3] = pdpte[3];

			utcb->mtd = Nova::Mtd::PDPTE | Nova::Mtd::FPU;

			Nova::reply(stack_reply);
		}

	public:

		Vcpu_handler_vmx(size_t stack_size, const pthread_attr_t *attr,
		                 void *(*start_routine) (void *), void *arg,
		                 Genode::Cpu_session * cpu_session,
		                 Genode::Affinity::Location location,
		                 unsigned int cpu_id, const char * name)
		:
			 Vcpu_handler(stack_size, attr, start_routine, arg, cpu_session, 
			              location, cpu_id, name)
		{
			using namespace Nova;

			typedef Vcpu_handler_vmx This;

			Genode::addr_t const exc_base = vcpu().exc_base();

			register_handler<VMX_EXIT_TRIPLE_FAULT, This,
				&This::_vmx_triple> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_INIT_SIGNAL, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_INT_WINDOW, This,
				&This::_vmx_irqwin> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_TASK_SWITCH, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_CPUID, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_HLT, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);

			/* we don't support tsc offsetting for now - so let the rdtsc exit */
			register_handler<VMX_EXIT_RDTSC, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);

			register_handler<VMX_EXIT_VMCALL, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_IO_INSTR, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_RDMSR, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_WRMSR, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_ERR_INVALID_GUEST_STATE, This,
				&This::_vmx_invalid> (exc_base, Mtd::ALL | Mtd::FPU);
//			register_handler<VMX_EXIT_PAUSE, This,
//				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_WBINVD, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_MOV_CRX, This,
				&This::_vmx_mov_crx> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_MOV_DRX, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_TPR_BELOW_THRESHOLD, This,
				&This::_vmx_default> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VMX_EXIT_EPT_VIOLATION, This,
				&This::_vmx_ept<VMX_EXIT_EPT_VIOLATION>> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<VCPU_STARTUP, This,
				&This::_vmx_startup> (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<RECALL, This,
				&This::_vmx_recall> (exc_base, Mtd::ALL | Mtd::FPU);

			start();
		}

		bool hw_save_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return vmx_save_state(utcb, pVM, pVCpu);
		}

		bool hw_load_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return vmx_load_state(utcb, pVM, pVCpu);
		}

		bool vm_exit_requires_instruction_emulation()
		{
			if (exit_reason == VMX_EXIT_TPR_BELOW_THRESHOLD) {
				/* the instruction causing the exit has already been executed */
				return false;
			}

			if (exit_reason == RECALL)
				return false;

			return true;
		}
};

#endif /* _VIRTUALBOX__SPEC__NOVA__VCPU_VMX_H_ */
