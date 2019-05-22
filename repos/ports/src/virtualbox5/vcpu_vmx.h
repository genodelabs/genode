/*
 * \brief  Genode specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__VCPU_VMX_H_
#define _VIRTUALBOX__VCPU_VMX_H_

/* base includes */
#include <vm_session/connection.h>
#include <vm_session/vm_session.h>

#include <cpu/vm_state.h>

/* libc includes */
#include <stdlib.h>

/* VirtualBox includes */
#include <VBox/vmm/hm_vmx.h>

/* Genode's VirtualBox includes */
#include "vcpu.h"
#include "vmx.h"


class Vcpu_handler_vmx : public Vcpu_handler
{
	private:

		Genode::Vm_handler<Vcpu_handler_vmx>  _handler;

		Genode::Vm_connection                &_vm_session;
		Genode::Vm_session_client::Vcpu_id    _vcpu;

		Genode::Attached_dataspace            _state_ds;

		template <unsigned X>
		void _vmx_ept()
		{
			Genode::addr_t const exit_qual = _state->qual_primary.value();
			Genode::addr_t const exit_addr = _state->qual_secondary.value();
			bool           const unmap     = exit_qual & 0x38;

			RTGCUINT vbox_errorcode = 0;
			if (exit_qual & VMX_EXIT_QUALIFICATION_EPT_INSTR_FETCH)
				vbox_errorcode |= X86_TRAP_PF_ID;
			if (exit_qual & VMX_EXIT_QUALIFICATION_EPT_DATA_WRITE)
				vbox_errorcode |= X86_TRAP_PF_RW;
			if (exit_qual & VMX_EXIT_QUALIFICATION_EPT_ENTRY_PRESENT)
				vbox_errorcode |= X86_TRAP_PF_P;

			npt_ept_exit_addr = exit_addr;
			npt_ept_unmap     = unmap;
			npt_ept_errorcode = vbox_errorcode;

			_npt_ept();
		}

		void _vmx_default() { _default_handler(); }

		void _vmx_startup()
		{
			/* configure VM exits to get */
			/* from src/VBox/VMM/VMMR0/HWVMXR0.cpp of virtualbox sources  */
			next_utcb.ctrl[0] = VMX_VMCS_CTRL_PROC_EXEC_HLT_EXIT |
			                    VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT |
			                    VMX_VMCS_CTRL_PROC_EXEC_UNCOND_IO_EXIT |
/*
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
			                    VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP |
			                    VMX_VMCS_CTRL_PROC_EXEC2_EPT |
			                    VMX_VMCS_CTRL_PROC_EXEC2_INVPCID;
		}

		void _vmx_triple()
		{
			Genode::error("triple fault - dead");
			exit(-1);
		}

		void _vmx_irqwin() { _irq_window(); }

		__attribute__((noreturn)) void _vmx_invalid()
		{
			unsigned const dubious = _state->inj_info.value() |
			                         _state->intr_state.value() |
			                         _state->actv_state.value();
			if (dubious)
				Genode::warning(__func__, " - dubious -"
				                " inj_info=", Genode::Hex(_state->inj_info.value()),
				                " inj_error=", Genode::Hex(_state->inj_error.value()),
				                " intr_state=", Genode::Hex(_state->intr_state.value()),
				                " actv_state=", Genode::Hex(_state->actv_state.value()));

			Genode::error("invalid guest state - dead");
			exit(-1);
		}

		void _vmx_mov_crx() { _default_handler(); return; }

		void _handle_vm_exception()
		{
			unsigned const exit = _state->exit_reason;
			bool recall_wait = true;

			switch (exit) {
			case VMX_EXIT_TRIPLE_FAULT: _vmx_triple(); break;
			case VMX_EXIT_INIT_SIGNAL: _vmx_default(); break;
			case VMX_EXIT_INT_WINDOW: _vmx_irqwin(); break;
			case VMX_EXIT_TASK_SWITCH: _vmx_default(); break;
			case VMX_EXIT_CPUID: _vmx_default(); break;
			case VMX_EXIT_HLT: _vmx_default(); break;
			/* we don't support tsc offsetting for now - so let the rdtsc exit */
			case VMX_EXIT_RDTSC: _vmx_default(); break;
			case VMX_EXIT_RDTSCP: _vmx_default(); break;
			case VMX_EXIT_VMCALL: _vmx_default(); break;
			case VMX_EXIT_IO_INSTR: _vmx_default(); break;
			case VMX_EXIT_RDMSR: _vmx_default(); break;
			case VMX_EXIT_WRMSR: _vmx_default(); break;
			case VMX_EXIT_ERR_INVALID_GUEST_STATE: _vmx_invalid(); break;
			case VMX_EXIT_PAUSE: _vmx_default(); break;
			case VMX_EXIT_WBINVD:  _vmx_default(); break;
			case VMX_EXIT_MOV_CRX: _vmx_mov_crx(); break;
			case VMX_EXIT_MOV_DRX: _vmx_default(); break;
			case VMX_EXIT_XSETBV: _vmx_default(); break;
			case VMX_EXIT_TPR_BELOW_THRESHOLD: _vmx_default(); break;
			case VMX_EXIT_EPT_VIOLATION: _vmx_ept<VMX_EXIT_EPT_VIOLATION>(); break;
			case RECALL:
				recall_wait = Vcpu_handler::_recall_handler();
				break;
			case VCPU_STARTUP:
				_vmx_startup();
				_lock_emt.unlock();
				/* pause - no resume */
				break;
			default:
				Genode::error(__func__, " unknown exit - stop - ",
				              Genode::Hex(exit));
				_vm_state = PAUSED;
				return;
			}

			if (exit == RECALL && !recall_wait) {
				_vm_state = RUNNING;
				run_vm();
				return;
			}

			/* wait until EMT thread wake's us up */
			_sem_handler.down();

			/* resume vCPU */
			_vm_state = RUNNING;
			if (_next_state == RUN)
				run_vm();
			else
				pause_vm(); /* cause pause exit */
		}

		void run_vm()   { _vm_session.run(_vcpu); }
		void pause_vm() { _vm_session.pause(_vcpu); }

		int attach_memory_to_vm(RTGCPHYS const gp_attach_addr,
		                        RTGCUINT vbox_errorcode)
		{
			return map_memory(_vm_session, gp_attach_addr, vbox_errorcode);
		}

		void _exit_config(Genode::Vm_state &state, unsigned exit)
		{
			switch (exit) {
			case VMX_EXIT_TRIPLE_FAULT:
			case VMX_EXIT_INIT_SIGNAL:
			case VMX_EXIT_INT_WINDOW:
			case VMX_EXIT_TASK_SWITCH:
			case VMX_EXIT_CPUID:
			case VMX_EXIT_HLT:
			case VMX_EXIT_RDTSC:
			case VMX_EXIT_RDTSCP:
			case VMX_EXIT_VMCALL:
			case VMX_EXIT_IO_INSTR:
			case VMX_EXIT_RDMSR:
			case VMX_EXIT_WRMSR:
			case VMX_EXIT_ERR_INVALID_GUEST_STATE:
//			case VMX_EXIT_PAUSE:
			case VMX_EXIT_WBINVD:
			case VMX_EXIT_MOV_CRX:
			case VMX_EXIT_MOV_DRX:
			case VMX_EXIT_TPR_BELOW_THRESHOLD:
			case VMX_EXIT_EPT_VIOLATION:
			case VMX_EXIT_XSETBV:
			case VCPU_STARTUP:
			case RECALL:
				/* todo - touch all members */
				Genode::memset(&state, ~0U, sizeof(state));
				break;
			default:
				break;
			}
		}

	public:

		Vcpu_handler_vmx(Genode::Env &env, size_t stack_size,
		                 Genode::Affinity::Location location,
		                 unsigned int cpu_id,
		                 Genode::Vm_connection &vm_session,
		                 Genode::Allocator &alloc)
		:
			 Vcpu_handler(env, stack_size, location, cpu_id),
			_handler(_ep, *this, &Vcpu_handler_vmx::_handle_vm_exception,
			         &Vcpu_handler_vmx::_exit_config),
			_vm_session(vm_session),
			/* construct vcpu */
			_vcpu(_vm_session.with_upgrade([&]() {
				return _vm_session.create_vcpu(alloc, env, _handler); })),
			/* get state of vcpu */
			_state_ds(env.rm(), _vm_session.cpu_state(_vcpu))
		{
			_state = _state_ds.local_addr<Genode::Vm_state>();

			/* sync with initial startup exception */
			_lock_emt.lock();

			_vm_session.run(_vcpu);

			/* sync with initial startup exception */
			_lock_emt.lock();
		}

		bool hw_save_state(Genode::Vm_state *state, VM * pVM, PVMCPU pVCpu) {
			return vmx_save_state(state, pVM, pVCpu);
		}

		bool hw_load_state(Genode::Vm_state * state, VM * pVM, PVMCPU pVCpu) {
			return vmx_load_state(state, pVM, pVCpu);
		}

		int vm_exit_requires_instruction_emulation(PCPUMCTX pCtx)
		{
			switch (_state->exit_reason) {
			case VMX_EXIT_HLT:
				pCtx->rip++;
				return VINF_EM_HALT;
			case VMX_EXIT_IO_INSTR:
				/* EMHandleRCTmpl.h does not distinguish READ/WRITE rc */
				return VINF_IOM_R3_IOPORT_WRITE;
			case VMX_EXIT_RDMSR:
				return VINF_CPUM_R3_MSR_READ;
			case VMX_EXIT_WRMSR:
				return VINF_CPUM_R3_MSR_WRITE;
			case VMX_EXIT_TPR_BELOW_THRESHOLD:
				/* the instruction causing the exit has already been executed */
			case RECALL:
				return VINF_SUCCESS;
			case VMX_EXIT_EPT_VIOLATION:
				if (_ept_fault_addr_type == PGMPAGETYPE_MMIO)
					/* EMHandleRCTmpl.h does not distinguish READ/WRITE rc */
					return VINF_IOM_R3_MMIO_READ_WRITE;
			case VMX_EXIT_MOV_DRX:
				/* looks complicated in original R0 code -> emulate instead */
				return VINF_EM_RAW_EMULATE_INSTR;
			default:
				return VINF_EM_RAW_EMULATE_INSTR;
			}
		}
};

#endif /* _VIRTUALBOX__VCPU_VMX_H_ */
