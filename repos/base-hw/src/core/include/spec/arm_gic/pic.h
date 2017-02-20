/*
 * \brief  Programmable interrupt controller for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__ARM_GIC__PIC_H_
#define _CORE__INCLUDE__SPEC__ARM_GIC__PIC_H_

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Genode
{
	/**
	 * Disributor of the ARM generic interrupt controller
	 */
	class Arm_gic_distributor;

	/**
	 * CPU interface of the ARM generic interrupt controller
	 */
	class Arm_gic_cpu_interface;

	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

namespace Kernel { using Pic = Genode::Pic; }


class Genode::Arm_gic_distributor : public Mmio
{
	public:

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
		struct Typer : Register<0x004, 32> {
			struct It_lines_number : Bitfield<0,5> { }; };

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

		/**
		 * Constructor
		 */
		Arm_gic_distributor(addr_t const base) : Mmio(base) { }

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

class Genode::Arm_gic_cpu_interface : public Mmio
{
	public:

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

		/**
		 * Constructor
		 */
		Arm_gic_cpu_interface(addr_t const base) : Mmio(base) { }
};

class Genode::Pic
{
	public:

		enum { IPI = 1 };

	protected:

		typedef Arm_gic_cpu_interface Cpui;
		typedef Arm_gic_distributor   Distr;

		static constexpr unsigned min_spi     = 32;
		static constexpr unsigned spurious_id = 1023;

		Distr               _distr;
		Cpui                _cpui;
		Cpui::Iar::access_t _last_iar;
		unsigned const      _max_irq;

		void _init();

		bool _valid(unsigned const irq_id) const { return irq_id <= _max_irq; }

	public:

		enum { NR_OF_IRQ = Distr::nr_of_irq };

		Pic();

		/**
		 * Initialize CPU local interface of the controller
		 */
		void init_cpu_local();

		/**
		 * Try to take an IRQ and return wether it was successful
		 *
		 * \param irq  contains kernel name of taken IRQ on success
		 */
		bool take_request(unsigned & irq)
		{
			_last_iar = _cpui.read<Cpui::Iar>();
			irq = Cpui::Iar::Irq_id::get(_last_iar);
			return _valid(irq);
		}

		/**
		 * End the last taken IRQ
		 */
		void finish_request()
		{
			_cpui.write<Cpui::Eoir>(_last_iar);
			_last_iar = Cpui::Iar::Irq_id::bits(spurious_id);
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
			_distr.write<Distr::Itargetsr::Cpu_targets>(targets, irq_id);
			_distr.write<Distr::Isenabler::Set_enable>(1, irq_id);
		}

		/**
		 * Mask IRQ with kernel name 'irq_id'
		 */
		void mask(unsigned const irq_id) {
			_distr.write<Distr::Icenabler::Clear_enable>(1, irq_id); }

		/**
		 * Raise inter-processor IRQ of the CPU with kernel name 'cpu_id'
		 */
		void send_ipi(unsigned const cpu_id)
		{
			typedef Distr::Sgir Sgir;
			Sgir::access_t sgir = 0;
			Sgir::Sgi_int_id::set(sgir, IPI);
			Sgir::Cpu_target_list::set(sgir, 1 << cpu_id);
			_distr.write<Sgir>(sgir);
		}

		/**
		 * Raise inter-processor interrupt on all other cores
		 */
		void send_ipi()
		{
			typedef Distr::Sgir Sgir;
			Sgir::access_t sgir = 0;
			Sgir::Sgi_int_id::set(sgir, IPI);
			Sgir::Target_list_filter::set(sgir,
			                              Sgir::Target_list_filter::ALL_OTHER);
			_distr.write<Sgir>(sgir);
		}
};

#endif /* _CORE__INCLUDE__SPEC__ARM_GIC__PIC_H_ */
