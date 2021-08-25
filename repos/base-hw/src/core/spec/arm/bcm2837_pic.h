/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2019-05-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM__BCM2837_PIC_H_
#define _CORE__SPEC__ARM__BCM2837_PIC_H_

#include <util/mmio.h>

#include <spec/arm/bcm2835_pic.h>

namespace Board {

	/* Global_interrupt_controller from Bcm2835_pic */
	class Bcm2837_pic;
}


class Board::Bcm2837_pic : Genode::Mmio
{
	public:

		enum {
			IPI       = 0,
			NR_OF_IRQ = 64,
		};

	private:

		template <unsigned CPU_NUM>
		struct Core_timer_irq_control : Register<0x40+CPU_NUM*0x4, 32>
		{
			struct Cnt_p_ns_irq
			: Register<0x40+CPU_NUM*0x4, 32>::template Bitfield<1, 1> {};
		};

		template <unsigned CPU_NUM>
		struct Core_mailbox_irq_control : Register<0x50+CPU_NUM*0x4, 32> {};

		template <unsigned CPU_NUM>
		struct Core_irq_source : Register<0x60+CPU_NUM*0x4, 32> {
			struct CntPsIrq : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<0, 1> { };
			struct CntPnIrq : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<1, 1> { };
			struct CntHpIrq : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<2, 1> { };
			struct CntVIrq  : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<3, 1> { };
			struct MBox0    : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<4, 1> { };
			struct MBox1    : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<5, 1> { };
			struct MBox2    : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<6, 1> { };
			struct MBox3    : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<7, 1> { };
			struct Gpu      : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<8, 1> { };
			struct Pmu      : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<9, 1> { };
			struct Axi      : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<10, 1> { };
			struct Timer    : Register<0x60+CPU_NUM*0x4, 32>::template Bitfield<11, 1> { };
		};

		template <unsigned CPU_NUM>
		struct Core_mailbox_set : Register<0x80+CPU_NUM*0x10, 32> {};

		template <unsigned CPU_NUM>
		struct Core_mailbox_clear : Register<0xc0+CPU_NUM*0x10, 32> {};

		void _ipi(unsigned cpu, bool enable);
		void _timer_irq(unsigned cpu, bool enable);

		Bcm2835_pic _bcm2835_pic;

	public:

		Bcm2837_pic(Global_interrupt_controller &);

		bool take_request(unsigned &irq);
		void finish_request() { }
		void unmask(unsigned const i, unsigned);
		void mask(unsigned const i);
		void irq_mode(unsigned, unsigned, unsigned);
		void send_ipi(unsigned);

		static constexpr bool fast_interrupts() { return false; }
};

#endif /* _CORE__SPEC__ARM__BCM2837_PIC_H_ */
