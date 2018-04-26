/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \date   2013-11-18
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__NOVA__VCPU_SVM_H_
#define _VIRTUALBOX__SPEC__NOVA__VCPU_SVM_H_

/* Genode's VirtualBox includes */
#include "vcpu.h"
#include "svm.h"

class Vcpu_handler_svm : public Vcpu_handler
{
	private:

		__attribute__((noreturn)) void _svm_default() { _default_handler(); }

		__attribute__((noreturn)) void _svm_vintr() { _irq_window(); }

		__attribute__((noreturn)) void _svm_ioio()
		{
			using namespace Nova;
			using namespace Genode;

			Thread *myself = Thread::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			if (utcb->qual[0] & 0x4) {
				unsigned ctrl0 = utcb->ctrl[0];

				Vmm::warning("invalid gueststate");

				utcb->ctrl[0] = ctrl0;
				utcb->ctrl[1] = 0;
				utcb->mtd = Mtd::CTRL;
				
				Nova::reply(_stack_reply);
			}

			_default_handler();
		}

		template <unsigned X>
		__attribute__((noreturn)) void _svm_npt()
		{
			using namespace Nova;
			using namespace Genode;

			Thread *myself = Thread::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			_exc_memory<X>(myself, utcb, utcb->qual[0] & 1,
			               utcb->qual[1], utcb->qual[0]);
		}

		__attribute__((noreturn)) void _svm_startup()
		{
			using namespace Nova;

			/* enable VM exits for CPUID */
			next_utcb.mtd     = Nova::Mtd::CTRL;
			next_utcb.ctrl[0] = SVM_CTRL1_INTERCEPT_CPUID;
			next_utcb.ctrl[1] = 0;

			void *exit_status = _start_routine(_start_routine_arg);
			pthread_exit(exit_status);

			Nova::reply(nullptr);
		}

		__attribute__((noreturn)) void _svm_recall()
		{
			Vcpu_handler::_recall_handler();
		}

	public:

		Vcpu_handler_svm(Genode::Env &env, size_t stack_size,
		                 void *(*start_routine) (void *), void *arg,
		                 Genode::Cpu_session * cpu_session,
		                 Genode::Affinity::Location location,
		                 unsigned int cpu_id, const char * name,
		                 Genode::Pd_session_capability pd_vcpu)
		:
			Vcpu_handler(env, stack_size, start_routine, arg, cpu_session,
			             location, cpu_id, name, pd_vcpu)
		{
			using namespace Nova;

			Genode::addr_t const exc_base = vcpu().exc_base();

			typedef Vcpu_handler_svm This;

			register_handler<RECALL, This,
				&This::_svm_recall>       (exc_base, Mtd::ALL | Mtd::FPU);
			register_handler<SVM_EXIT_IOIO, This,
				&This::_svm_ioio>         (exc_base, Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_VINTR, This,
				&This::_svm_vintr>        (exc_base, Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_RDTSC, This,
				&This::_svm_default>      (exc_base, Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_MSR, This,
				&This::_svm_default>      (exc_base, Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_NPT, This,
				&This::_svm_npt<SVM_NPT>> (exc_base, Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_HLT, This,
				&This::_svm_default>      (exc_base, Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_CPUID, This,
				&This::_svm_default>      (exc_base, Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<VCPU_STARTUP, This,
				&This::_svm_startup>      (exc_base, Mtd(Mtd::ALL | Mtd::FPU));

			start();
		}

		bool hw_save_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return svm_save_state(utcb, pVM, pVCpu);
		}

		bool hw_load_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return svm_load_state(utcb, pVM, pVCpu);
		}

		int vm_exit_requires_instruction_emulation(PCPUMCTX)
		{
			if (exit_reason == RECALL)
				return VINF_SUCCESS;

			return VINF_EM_RAW_EMULATE_INSTR;
		}
};

#endif /* _VIRTUALBOX__SPEC__NOVA__VCPU_SVM_H_ */
