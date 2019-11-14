/*
 * \brief  VMM cpu object
 * \author Stefan Kalkowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__CPU_H_
#define _SRC__SERVER__VMM__CPU_H_

#include <cpu_base.h>

namespace Vmm { class Cpu; }

class Vmm::Cpu : public Vmm::Cpu_base
{
	public:

		Cpu(Vm                      & vm,
		    Genode::Vm_connection   & vm_session,
		    Mmio_bus                & bus,
		    Gic                     & gic,
		    Genode::Env             & env,
		    Genode::Heap            & heap,
		    Genode::Entrypoint      & ep);

		enum Exception_type {
			NO_EXCEPTION,
			RESET,
			UNDEFINED,
			HVC,
			PF_ABORT,
			DATA_ABORT,
			IRQ,
			FIQ,
			TRAP
		};

	private:

		struct Ccsidr : System_register
		{
			System_register & csselr;

			Ccsidr(System_register &csselr,
			       Genode::Avl_tree<System_register> & tree)
			: System_register(0, 1, 0, 0, "CCSIDR", false, 0x0, tree),
			  csselr(csselr) {}

			virtual Genode::addr_t read() const override;
		};

		/******************************
		 ** Identification registers **
		 ******************************/

		System_register _sr_midr;
		System_register _sr_mpidr;
		System_register _sr_mmfr0;
		System_register _sr_mmfr1;
		System_register _sr_mmfr2;
		System_register _sr_mmfr3;
		System_register _sr_isar0;
		System_register _sr_isar1;
		System_register _sr_isar2;
		System_register _sr_isar3;
		System_register _sr_isar4;
		System_register _sr_isar5;
		System_register _sr_pfr0;
		System_register _sr_pfr1;
		System_register _sr_clidr;
		System_register _sr_csselr;
		System_register _sr_ctr;
		System_register _sr_revidr;
		Ccsidr          _sr_ccsidr;


		/*********************
		 ** System register **
		 *********************/

		System_register _sr_actlr;
};

#endif /* _SRC__SERVER__VMM__CPU_H_ */
