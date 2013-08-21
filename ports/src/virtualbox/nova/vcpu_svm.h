/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \date   2013-11-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode's VirtualBox includes */
#include "vcpu.h"
#include "svm.h"

class Vcpu_handler_svm : public Vcpu_handler
{
	private:

		__attribute__((noreturn)) void _svm_vintr() {
			_default_handler(SVM_EXIT_VINTR);
		}
		__attribute__((noreturn)) void _svm_rdtsc() {
			_default_handler(SVM_EXIT_RDTSC);
		}

		__attribute__((noreturn)) void _svm_msr() {
			_default_handler(SVM_EXIT_MSR);
		}

		__attribute__((noreturn)) void _svm_recall()
		{
			_default_handler(SVM_INVALID);
		}

		__attribute__((noreturn)) void _svm_halt()
		{
			_default_handler(SVM_EXIT_HLT);
		}

		__attribute__((noreturn)) void _svm_ioio()
		{
			using namespace Nova;
			using namespace Genode;

			Thread_base *myself = Thread_base::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			if (utcb->qual[0] & 0x4) {
				unsigned ctrl0 = utcb->ctrl[0];

				PERR("invalid gueststate");

				/* deadlock here */
				_signal_vcpu.lock();


				utcb->ctrl[0] = ctrl0;
				utcb->ctrl[1] = 0;
				utcb->mtd = Mtd::CTRL;
				
				Nova::reply(myself->stack_top());
			}

			_default_handler(SVM_EXIT_IOIO);
		}

		template <unsigned X>
		__attribute__((noreturn)) void _svm_npt()
		{
			using namespace Nova;
			using namespace Genode;

			Thread_base *myself = Thread_base::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			_exc_memory<X>(myself, utcb, utcb->qual[0] & 1,
			               utcb->qual[1] & ~((1UL << 12) - 1));
		}

		__attribute__((noreturn)) void _svm_startup()
		{
			using namespace Nova;

			Genode::Thread_base *myself = Genode::Thread_base::myself();
			Utcb *utcb = reinterpret_cast<Utcb *>(myself->utcb());

			/* we are ready, unlock our creator */
			_lock_startup.unlock();

			/* wait until EMT thread say so */
			_signal_vcpu.lock();

			Nova::reply(myself->stack_top());
		}

	public:

		Vcpu_handler_svm()
		{
			using namespace Nova;

			typedef Vcpu_handler_svm This;

			register_handler<RECALL, This,
				&This::_svm_recall>(vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_IOIO, This,
				&This::_svm_ioio> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_VINTR, This,
				&This::_svm_vintr> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_RDTSC, This,
				&This::_svm_rdtsc> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_MSR, This,
				&This::_svm_msr> (vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_NPT, This,
				&This::_svm_npt<SVM_NPT>>(vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<SVM_EXIT_HLT, This,
				&This::_svm_halt>(vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));
			register_handler<VCPU_STARTUP, This,
				&This::_svm_startup>(vcpu().exc_base(), Mtd(Mtd::ALL | Mtd::FPU));

			start();
		}

		bool hw_save_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return svm_save_state(utcb, pVM, pVCpu);
		}

		bool hw_load_state(Nova::Utcb * utcb, VM * pVM, PVMCPU pVCpu) {
			return svm_load_state(utcb, pVM, pVCpu);
		}
};
