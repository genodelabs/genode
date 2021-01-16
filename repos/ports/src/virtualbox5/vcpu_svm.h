/*
 * \brief  Genode specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \date   2013-11-18
 */

/*
 * Copyright (C) 2013-2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__VCPU_SVM_H_
#define _VIRTUALBOX__VCPU_SVM_H_

/* base includes */
#include <vm_session/connection.h>
#include <vm_session/vm_session.h>

#include <cpu/vm_state.h>

/* Genode's VirtualBox includes */
#include "vcpu.h"
#include "svm.h"

class Vcpu_handler_svm : public Vcpu_handler
{
	private:

		Genode::Vm_handler<Vcpu_handler_svm>  _handler;

		Genode::Vm_connection                &_vm_session;
		Genode::Vm_session_client::Vcpu_id    _vcpu;

		Genode::Attached_dataspace            _state_ds;

		void _svm_default() { _default_handler(); }
		void _svm_vintr()   { _irq_window(); }

		template <unsigned X>
		void _svm_npt()
		{
			bool           const unmap          = _state->qual_primary.value() & 1;
			Genode::addr_t const exit_addr      = _state->qual_secondary.value();
			RTGCUINT       const vbox_errorcode = _state->qual_primary.value();

			npt_ept_exit_addr = exit_addr;
			npt_ept_unmap     = unmap;
			npt_ept_errorcode = vbox_errorcode;

			_npt_ept();
		}

		void _svm_startup()
		{
			/* enable VM exits */
			next_utcb.ctrl[0] = SVM_CTRL1_INTERCEPT_INTR
			                  | SVM_CTRL1_INTERCEPT_NMI
			                  | SVM_CTRL1_INTERCEPT_INIT
			                  | SVM_CTRL1_INTERCEPT_RDPMC
			                  | SVM_CTRL1_INTERCEPT_CPUID
			                  | SVM_CTRL1_INTERCEPT_RSM
			                  | SVM_CTRL1_INTERCEPT_HLT
			                  | SVM_CTRL1_INTERCEPT_INOUT_BITMAP
			                  | SVM_CTRL1_INTERCEPT_MSR_SHADOW
			                  | SVM_CTRL1_INTERCEPT_INVLPGA
			                  | SVM_CTRL1_INTERCEPT_SHUTDOWN
			                  | SVM_CTRL1_INTERCEPT_RDTSC
			                  | SVM_CTRL1_INTERCEPT_FERR_FREEZE;

			next_utcb.ctrl[1] = SVM_CTRL2_INTERCEPT_VMRUN
			                  | SVM_CTRL2_INTERCEPT_VMMCALL
			                  | SVM_CTRL2_INTERCEPT_VMLOAD
			                  | SVM_CTRL2_INTERCEPT_VMSAVE
			                  | SVM_CTRL2_INTERCEPT_STGI
			                  | SVM_CTRL2_INTERCEPT_CLGI
			                  | SVM_CTRL2_INTERCEPT_SKINIT
			                  | SVM_CTRL2_INTERCEPT_WBINVD
			                  | SVM_CTRL2_INTERCEPT_MONITOR
			                  | SVM_CTRL2_INTERCEPT_RDTSCP
			                  | SVM_CTRL2_INTERCEPT_MWAIT;
		}

		void _handle_vm_exception()
		{
			unsigned const exit = _state->exit_reason;
			bool recall_wait = true;

			switch (exit) {
			case SVM_EXIT_VINTR: _svm_vintr(); break;
			case SVM_NPT:        _svm_npt<SVM_NPT>(); break;
			case SVM_EXIT_CPUID:
			case SVM_EXIT_HLT:
			case SVM_EXIT_INVLPGA:
			case SVM_EXIT_IOIO:
			case SVM_EXIT_MSR:
			case SVM_EXIT_READ_CR0 ... SVM_EXIT_WRITE_CR15:
			case SVM_EXIT_RDTSC:
			case SVM_EXIT_RDTSCP:
			case SVM_EXIT_WBINVD:
				_svm_default();
				break;
			case SVM_INVALID:
				Genode::warning("invalid svm ip=", _state->ip.value());
				_svm_default();
				break;
			case SVM_EXIT_SHUTDOWN:
				Genode::error("shutdown exit");
				::exit(-1);
				break;
			case RECALL:
				recall_wait = Vcpu_handler::_recall_handler();
				break;
			case VCPU_STARTUP:
				_svm_startup();
				_blockade_emt.wakeup();
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
			case RECALL:
			case SVM_EXIT_INVLPGA:
			case SVM_EXIT_IOIO:
			case SVM_EXIT_VINTR:
			case SVM_EXIT_READ_CR0 ... SVM_EXIT_WRITE_CR15:
			case SVM_EXIT_RDTSC:
			case SVM_EXIT_RDTSCP:
			case SVM_EXIT_MSR:
			case SVM_NPT:
			case SVM_EXIT_HLT:
			case SVM_EXIT_CPUID:
			case SVM_EXIT_WBINVD:
			case VCPU_STARTUP:
				/* todo - touch all members */
				Genode::memset(&state, ~0U, sizeof(state));
				break;
			default:
				break;
			}
		}

	public:

		Vcpu_handler_svm(Genode::Env &env, size_t stack_size,
		                 Genode::Affinity::Location location,
		                 unsigned int cpu_id,
		                 Genode::Vm_connection &vm_session,
		                 Genode::Allocator &alloc)
		:
			 Vcpu_handler(env, stack_size, location, cpu_id),
			_handler(_ep, *this, &Vcpu_handler_svm::_handle_vm_exception,
			         &Vcpu_handler_svm::_exit_config),
			_vm_session(vm_session),
			/* construct vcpu */
			_vcpu(_vm_session.with_upgrade([&]() {
				return _vm_session.create_vcpu(alloc, env, _handler); })),
			/* get state of vcpu */
			_state_ds(env.rm(), _vm_session.cpu_state(_vcpu))
		{
			_state = _state_ds.local_addr<Genode::Vm_state>();

			_vm_session.run(_vcpu);

			/* sync with initial startup exception */
			_blockade_emt.block();
		}

		bool hw_save_state(Genode::Vm_state *state, VM * pVM, PVMCPU pVCpu) {
			return svm_save_state(state, pVM, pVCpu);
		}

		bool hw_load_state(Genode::Vm_state *state, VM * pVM, PVMCPU pVCpu) {
			return svm_load_state(state, pVM, pVCpu);
		}

		int vm_exit_requires_instruction_emulation(PCPUMCTX)
		{
			if (_state->exit_reason == RECALL)
				return VINF_SUCCESS;

			return VINF_EM_RAW_EMULATE_INSTR;
		}
};

#endif /* _VIRTUALBOX__VCPU_SVM_H_ */
