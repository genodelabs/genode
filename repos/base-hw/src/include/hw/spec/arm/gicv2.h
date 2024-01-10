/*
 * \brief  ARM generic interrupt controller v2
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__GICv2_H_
#define _SRC__LIB__HW__SPEC__ARM__GICv2_H_

#include <util/mmio.h>

namespace Hw { class Gicv2; }


class Hw::Gicv2
{
	protected:

		/**
		 * Distributor of the ARM generic interrupt controller
		 */
		struct Distributor : Genode::Mmio<0xf04>
		{
			static constexpr unsigned nr_of_irq = 1024;

			/**
			 * Control register
			 */
			struct Ctlr : Register<0x000, 32>
			{
				struct Enable      : Bitfield<0,1> { };
				struct Enable_grp0 : Bitfield<0,1> { };
				struct Enable_grp1 : Bitfield<1,1> { };
			};

			/**
			 * Controller type register
			 */
			struct Typer : Register<0x004, 32>
			{
				struct It_lines_number    : Bitfield<0,5>  { };
				struct Security_extension : Bitfield<10,1> { };
			};

			/**
			 * Interrupt group register
			 */
			struct Igroupr : Register_array<0x80, 32, nr_of_irq, 1> {
				struct Group_status : Bitfield<0, 1> { }; };

			/**
			 * Interrupt set enable registers
			 */
			struct Isenabler : Register_array<0x100, 32, nr_of_irq, 1, true> {
				struct Set_enable : Bitfield<0, 1> { }; };

			/**
			 * Interrupt clear enable registers
			 */
			struct Icenabler : Register_array<0x180, 32, nr_of_irq, 1, true> {
				struct Clear_enable : Bitfield<0, 1> { }; };

			/**
			 * Interrupt priority level registers
			 */
			struct Ipriorityr : Register_array<0x400, 32, nr_of_irq, 8> {
				struct Priority : Bitfield<0, 8> { }; };

			/**
			 * Interrupt CPU-target registers
			 */
			struct Itargetsr : Register_array<0x800, 32, nr_of_irq, 8> {
				struct Cpu_targets : Bitfield<0, 8> { }; };

			/**
			 * Interrupt configuration registers
			 */
			struct Icfgr : Register_array<0xc00, 32, nr_of_irq, 2> {
				struct Edge_triggered : Bitfield<1, 1> { }; };

			/**
			 * Software generated interrupt register
			 */
			struct Sgir : Register<0xf00, 32>
			{
				struct Sgi_int_id      : Bitfield<0,  4> { };
				struct Cpu_target_list : Bitfield<16, 8> { };
				struct Target_list_filter : Bitfield<24, 2>
				{
					enum Target { TARGET_LIST, ALL_OTHER, MYSELF };
				};
			};

			Distributor(Genode::addr_t const base) : Mmio({(char *)base, Mmio::SIZE}) { }

			/**
			 * Return minimum IRQ priority
			 */
			unsigned min_priority()
			{
				write<Ipriorityr::Priority>(~0, 0);
				return read<Ipriorityr::Priority>(0);
			}

			/**
			 * Return highest IRQ number
			 */
			unsigned max_irq()
			{
				constexpr unsigned line_width_log2 = 5;
				Typer::access_t const lnr = read<Typer::It_lines_number>();
				return ((lnr + 1) << line_width_log2) - 1;
			}
		};


		/**
		 * CPU interface of the ARM generic interrupt controller
		 */
		struct Cpu_interface : Genode::Mmio<0x14>
		{
			/**
			 * Control register
			 */
			struct Ctlr : Register<0x00, 32>
			{
				struct Enable      : Bitfield<0,1> { };
				struct Enable_grp0 : Bitfield<0,1> { };
				struct Enable_grp1 : Bitfield<1,1> { };
				struct Fiq_en      : Bitfield<3,1> { };
			};

			/**
			 * Priority mask register
			 */
			struct Pmr : Register<0x04, 32> {
				struct Priority : Bitfield<0,8> { }; };

			/**
			 * Binary point register
			 */
			struct Bpr : Register<0x08, 32> {
				struct Binary_point : Bitfield<0,3> { }; };

			/**
			 * Interrupt acknowledge register
			 */
			struct Iar : Register<0x0c, 32, true> {
				struct Irq_id : Bitfield<0,10> { }; };

			/**
			 * End of interrupt register
			 */
			struct Eoir : Register<0x10, 32, true> {
				struct Irq_id : Bitfield<0,10> { }; };

			Cpu_interface(Genode::addr_t const base) : Mmio({(char *)base, Mmio::SIZE}) { }
		};


		static constexpr unsigned min_spi     = 32;
		static constexpr unsigned spurious_id = 1023;

		Distributor                  _distr;
		Cpu_interface                _cpui;
		Cpu_interface::Iar::access_t _last_iar;
		unsigned const               _max_irq;

		bool _valid(unsigned const irq_id) const { return irq_id <= _max_irq; }

	public:

		enum { IPI = 1, NR_OF_IRQ = Distributor::nr_of_irq };

		Gicv2();

		/**
		 * Try to take an IRQ and return wether it was successful
		 *
		 * \param irq  contains kernel name of taken IRQ on success
		 */
		bool take_request(unsigned & irq)
		{
			_last_iar = _cpui.read<Cpu_interface::Iar>();
			irq = Cpu_interface::Iar::Irq_id::get(_last_iar);
			return _valid(irq);
		}

		/**
		 * End the last taken IRQ
		 */
		void finish_request()
		{
			_cpui.write<Cpu_interface::Eoir>(_last_iar);
			_last_iar = Cpu_interface::Iar::Irq_id::bits(spurious_id);
		}

		/**
		 * Unmask IRQ and assign it to one CPU
		 *
		 * \param irq_id  kernel name of targeted IRQ
		 * \param cpu_id  kernel name of targeted CPU
		 */
		void unmask(unsigned const irq_id, unsigned const cpu_id)
		{
			unsigned const targets = 1 << cpu_id;
			_distr.write<Distributor::Itargetsr::Cpu_targets>(targets, irq_id);
			_distr.write<Distributor::Isenabler::Set_enable>(1, irq_id);
		}

		/**
		 * Mask IRQ with kernel name 'irq_id'
		 */
		void mask(unsigned const irq_id) {
			_distr.write<Distributor::Icenabler::Clear_enable>(1, irq_id); }

		/**
		 * Set trigger and polarity
		 */
		void irq_mode(unsigned, unsigned, unsigned) { }

		/**
		 * Raise inter-processor IRQ of the CPU with kernel name 'cpu_id'
		 */
		void send_ipi(unsigned const cpu_id)
		{
			using Sgir = Distributor::Sgir;
			Sgir::access_t sgir = 0;
			Sgir::Sgi_int_id::set(sgir, IPI);
			Sgir::Cpu_target_list::set(sgir, 1 << cpu_id);
			_distr.write<Sgir>(sgir);
		}

		static constexpr bool fast_interrupts() { return false; }
};

#endif /* _SRC__LIB__HW__SPEC__ARM__GICv2_H_ */
