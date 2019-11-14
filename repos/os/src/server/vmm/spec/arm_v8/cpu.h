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
			AARCH64_SYNC   = 0x400,
			AARCH64_IRQ    = 0x480,
			AARCH64_FIQ    = 0x500,
			AARCH64_SERROR = 0x580,
			AARCH32_SYNC   = 0x600,
			AARCH32_IRQ    = 0x680,
			AARCH32_FIQ    = 0x700,
			AARCH32_SERROR = 0x780,
			NO_EXCEPTION   = 0xffff
		};

	private:

		class Id_aa64pfr0 : public System_register,
		                    public Genode::Register<64>
		{
			private:

				struct El0 : Bitfield<0,  4> { enum { AARCH64_ONLY = 1 }; };
				struct El1 : Bitfield<4,  4> { enum { AARCH64_ONLY = 1 }; };
				struct El2 : Bitfield<8,  4> { enum { NOT_IMPLEMENTED  }; };
				struct El3 : Bitfield<12, 4> { enum { NOT_IMPLEMENTED  }; };
				struct Ras : Bitfield<28, 4> { enum { NOT_IMPLEMENTED  }; };
				struct Sve : Bitfield<32, 4> { enum { NOT_IMPLEMENTED  }; };

				access_t _reset_value(access_t orig)
				{
					El0::set(orig, El0::AARCH64_ONLY);
					El1::set(orig, El1::AARCH64_ONLY);
					El2::set(orig, El2::NOT_IMPLEMENTED);
					El3::set(orig, El3::NOT_IMPLEMENTED);
					Ras::set(orig, Ras::NOT_IMPLEMENTED);
					Sve::set(orig, Sve::NOT_IMPLEMENTED);
					return orig;
				}

			public:

				Id_aa64pfr0(Genode::uint64_t id_aa64pfr0,
				            Genode::Avl_tree<System_register> & tree)
				: System_register(3, 0, 0, 4, 0, "ID_AA64PFR0_EL1", false,
				                  _reset_value(id_aa64pfr0), tree) {}
		};

		struct Ccsidr : System_register
		{
			System_register & csselr;
			State           & state;

			Ccsidr(System_register &csselr,
			       State & state,
			       Genode::Avl_tree<System_register> & tree)
			: System_register(3, 0, 1, 0, 0, "CCSIDR_EL1", false, 0x0, tree),
			  csselr(csselr), state(state) {}

			virtual Genode::addr_t read() const override;
		};

		struct Ctr_el0 : System_register
		{
			Ctr_el0(Genode::Avl_tree<System_register> & tree)
			: System_register(3, 0, 3, 0, 1, "CTR_EL0", false, 0x0, tree) {}

			virtual Genode::addr_t read() const override;
		};

		struct Icc_sgi1r_el1 : System_register
		{
			Vm & vm;

			Icc_sgi1r_el1(Genode::Avl_tree<System_register> & tree, Vm & vm)
			: System_register(3, 12, 0, 11, 5, "SGI1R_EL1", true, 0x0, tree),
			  vm(vm) {}

			virtual void write(Genode::addr_t v) override;
		};

		/******************************
		 ** Identification registers **
		 ******************************/

		System_register                   _sr_id_aa64afr0_el1;
		System_register                   _sr_id_aa64afr1_el1;
		System_register                   _sr_id_aa64dfr0_el1;
		System_register                   _sr_id_aa64dfr1_el1;
		System_register                   _sr_id_aa64isar0_el1;
		System_register                   _sr_id_aa64isar1_el1;
		System_register                   _sr_id_aa64mmfr0_el1;
		System_register                   _sr_id_aa64mmfr1_el1;
		System_register                   _sr_id_aa64mmfr2_el1;
		Id_aa64pfr0                       _sr_id_aa64pfr0_el1;
		System_register                   _sr_id_aa64pfr1_el1;
		System_register                   _sr_id_aa64zfr0_el1;
		System_register                   _sr_aidr_el1;
		System_register                   _sr_revidr_el1;

		/*********************
		 ** Cache registers **
		 *********************/

		System_register                   _sr_clidr_el1;
		System_register                   _sr_csselr_el1;
		Ctr_el0                           _sr_ctr_el0;
		Ccsidr                            _sr_ccsidr_el1;

		/***********************************
		 ** Performance monitor registers **
		 ***********************************/

		System_register                   _sr_pmuserenr_el0;

		/*****************************
		 ** Debug monitor registers **
		 *****************************/

		System_register                   _sr_dbgbcr0;
		System_register                   _sr_dbgbvr0;
		System_register                   _sr_dbgwcr0;
		System_register                   _sr_dbgwvr0;
		System_register                   _sr_mdscr;
		System_register                   _sr_osdlr;
		System_register                   _sr_oslar;

		/***********************
		 ** GIC cpu interface **
		 ***********************/

		Icc_sgi1r_el1                     _sr_sgi1r_el1;
};

#endif /* _SRC__SERVER__VMM__CPU_H_ */
