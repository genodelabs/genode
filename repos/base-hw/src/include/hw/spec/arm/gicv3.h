/*
 * \brief  GICv3 interrupt controller for core
 * \author Sebastian Sumpf
 * \date   2019-07-08
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__GIC_V3_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__GIC_V3_H_

#include <util/mmio.h>

namespace Hw     { class Pic; }

#define SYSTEM_REGISTER(sz, name, reg, ...) \
	struct name : Genode::Register<sz> \
	{ \
		static access_t read() \
		{ \
			access_t v; \
			asm volatile ("mrs %0, " reg : "=r" (v)); \
			return v; \
		} \
 \
		static void write(access_t const v) { \
			asm volatile ("msr " reg ", %0" :: "r" (v)); } \
 \
		__VA_ARGS__; \
	};


class Hw::Pic
{
	protected:

		static constexpr unsigned min_spi     = 32;
		static constexpr unsigned spurious_id = 1023;

		struct Distributor : Genode::Mmio<0x7fe0>
		{
			static constexpr unsigned nr_of_irq = 1024;

			/**
			 * Control register (secure access)
			 */
			/* XXX: CAUTION this is different in EL3/EL2/EL1! */
			struct Ctlr : Register<0x0, 32>
			{
				struct Enable_grp0    : Bitfield<0, 1> { };
				struct Enable_grp1_a  : Bitfield<1, 1> { };
				struct Are_ns         : Bitfield<5, 1> { };
				struct Rwp            : Bitfield<31, 1> { };
			};

			struct Typer : Register<0x004, 32> {
				struct It_lines_number : Bitfield<0,5> { }; };

			struct Igroup0r : Register_array<0x80, 32, nr_of_irq, 1> {
				struct Group1 : Bitfield<0, 1> { }; };

			/**
			 * Interrupt Set-Enable register
			 */
			struct Isenabler : Register_array<0x100, 32, nr_of_irq, 1, true> {
				struct Set_enable : Bitfield<0, 1> { }; };

			/**
			 * Interrupt clear enable registers
			 */
			struct Icenabler : Register_array<0x180, 32, nr_of_irq, 1, true> {
				struct Clear_enable : Bitfield<0, 1> { }; };

			/**
			 * Interrupt clear pending registers
			 */
			struct Icpendr : Register_array<0x280, 32, nr_of_irq, 1, true> {
				struct Clear_pending : Bitfield<0, 1> { }; };

			/**
			 * Interrupt priority level registers
			 */
			struct Ipriorityr : Register_array<0x400, 32, nr_of_irq, 8> {
				struct Priority : Bitfield<0, 8> { }; };


			struct Icfgr : Register_array<0xc00, 32, nr_of_irq, 2> {
				struct Edge_triggered : Bitfield<1, 1> { }; };

			struct Irouter : Register_array<0x6000, 64, 1020, 64, true> { };

			void wait_for_rwp()
			{
				for (unsigned i = 0; i < 1000; i++)
					if (read<Ctlr::Rwp>() == 0)
						return;
			}

			unsigned max_irq() { return 32 * (read<Typer::It_lines_number>() + 1) - 1; }

			using Mmio::Mmio;
		};

		struct Redistributor : Genode::Mmio<0x4>
		{
			struct Ctlr : Register<0x0, 32>
			{
				struct Uwp : Bitfield<31, 1> { };
			};

			using Mmio::Mmio;

			/* wait for upstream writes */
			void wait_for_uwp()
			{
				for (unsigned i = 0; i < 1000; i++)
					if (read<Ctlr::Uwp>() == 0)
						return;
			}
		};

		struct Redistributor_sgi_ppi : Genode::Mmio<0xc08>
		{
			struct Igroupr0   : Register<0x80, 32> { };

			struct Isenabler0 : Register_array<0x100, 32, min_spi, 1, true>
			{ };

			struct Icenabler0 : Register_array<0x180, 32, min_spi, 1, true>
			{ };

			struct Icactiver0 : Register<0x380, 32, true> { };

			struct Ipriorityr : Register_array<0x400, 32, min_spi, 8> {
				struct Priority : Bitfield<0, 8> { }; };

			struct Icfgr1 : Register<0xc04, 32> { };

			using Mmio::Mmio;
		};

		struct Cpu_interface
		{
			SYSTEM_REGISTER(32, Icc_sre_el1, "S3_0_C12_C12_5",
				struct Sre : Bitfield<0, 1> { };
			);

			SYSTEM_REGISTER(32, Icc_iar1_el1,    "S3_0_C12_C12_0");
			SYSTEM_REGISTER(32, Icc_br1_el1,     "S3_0_C12_C12_3");
			SYSTEM_REGISTER(32, Icc_pmr_el1,     "S3_0_C4_C6_0");
			SYSTEM_REGISTER(32, Icc_igrpen1_el1, "S3_0_C12_C12_7");
			SYSTEM_REGISTER(32, Icc_eoir1_el1,   "S3_0_C12_C12_1");
			SYSTEM_REGISTER(64, Icc_sgi1r_el1,   "S3_0_C12_C11_5");

			void init()
			{
				/* enable register access */
				Icc_sre_el1::access_t sre = Icc_sre_el1::read();
				Icc_sre_el1::Sre::set(sre, 1);
				Icc_sre_el1::write(sre);

				/* XXX: check if needed or move somewhere else */
				asm volatile("isb sy" ::: "memory");

				/* no priority grouping */
				Icc_br1_el1::write(0);

				/* allow all priorities */
				Icc_pmr_el1::write(0xff);

				/* enable GRP1 interrupts */
				Icc_igrpen1_el1::write(1);

				/* XXX: check if needed or move somewhere else */
				asm volatile("isb sy" ::: "memory");
			}
		};

		void _redistributor_init()
		{
			/* diactivate SGI/PPI */
			_redistr_sgi.write<Redistributor_sgi_ppi::Icactiver0>(~0u);

			for (unsigned i = 0; i < min_spi; i++) {
				_redistr_sgi.write<Redistributor_sgi_ppi::Ipriorityr::Priority>(0xa0, i); }

			/* set group 1 for all PPI/SGIs */
			_redistr_sgi.write<Redistributor_sgi_ppi::Igroupr0>(~0);

			/* disable SGI/PPI */
			_redistr_sgi.write<Redistributor_sgi_ppi::Icenabler0>(~0);

			/* set PPIs to level triggered */
			_redistr_sgi.write<Redistributor_sgi_ppi::Icfgr1>(0);

			_redistr.wait_for_uwp();
		}

		Distributor           _distr;
		Redistributor         _redistr;
		Redistributor_sgi_ppi _redistr_sgi;
		Cpu_interface         _cpui { };

		unsigned const _max_irq;

		Cpu_interface::Icc_iar1_el1::access_t _last_iar { spurious_id };

		bool _valid(unsigned const irq_id) const { return irq_id <= _max_irq; }

	public:

		Pic();

		enum { IPI = 0 };
		enum { NR_OF_IRQ = Distributor::nr_of_irq };

		bool take_request(unsigned &irq)
		{
			_last_iar = Cpu_interface::Icc_iar1_el1::read();
			irq       = _last_iar;

			return _valid(irq);
		}

		void finish_request()
		{
			Cpu_interface::Icc_eoir1_el1::write(_last_iar);
			_last_iar = spurious_id;
		}

		void unmask(unsigned const irq_id, unsigned const /* cpu_id */)
		{
			if (irq_id < min_spi) {
				_redistr_sgi.write<Redistributor_sgi_ppi::Isenabler0>(1, irq_id);
			} else {
				_distr.write<Distributor::Isenabler::Set_enable>(1, irq_id);
			}
		}

		void mask(unsigned const irq_id)
		{
			if (irq_id < min_spi) {
				_redistr_sgi.write<Redistributor_sgi_ppi::Icenabler0>(1, irq_id);
			} else {
				_distr.write<Distributor::Icenabler::Clear_enable>(1, irq_id);
			}
		}

		void irq_mode(unsigned, unsigned, unsigned) { }

		void send_ipi(unsigned const cpu_id)
		{
			Cpu_interface::Icc_sgi1r_el1::write(1ULL << cpu_id);
		}
};

#undef SYSTEM_REGISTER

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__GIC_V3_H_ */
