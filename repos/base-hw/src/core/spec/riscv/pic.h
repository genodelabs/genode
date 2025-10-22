/*
 * \brief  Programmable interrupt controller for core
 * \author Sebastian Sumpf
 * \date   2015-06-02
 *
 * The platform specific PLIC-register layout can be found in 'plic.h'
 */

/*
 * Copyright (C) 2015-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__RISCV__PIC_H_
#define _CORE__SPEC__RISCV__PIC_H_

#include <irq_session/irq_session.h>
#include <util/mmio.h>
#include <plic.h>

#include <hw/spec/riscv/cpu.h>

namespace Board {

	struct Global_interrupt_controller
	{
		/* no suspend/resume on this platform, leave it empty */
		void resume() {}

		static constexpr unsigned NR_OF_IRQ = Plic::NR_OF_IRQ;
	};
	class Local_interrupt_controller;
}


/**
 * IC driver for core for one CPU
 */
class Board::Local_interrupt_controller
{
	private:

		Plic               _plic;
		Plic::Id::access_t _last_irq { 0 };

	public:

		enum {
			/* Dummy IPI value of uni-processor platform */
			IPI = Global_interrupt_controller::NR_OF_IRQ + 1,
		};

		Local_interrupt_controller(Global_interrupt_controller &);

		bool take_request(unsigned &irq)
		{
			irq = _plic.read<Plic::Id>();
			if (irq == 0) return false;
			_last_irq = irq;
			return true;
		}

		void finish_request()
		{
			_plic.write<Plic::Id>(_last_irq);
		}

		void unmask(unsigned irq, Hw::Riscv_cpu::Id)
		{
			if (irq > Global_interrupt_controller::NR_OF_IRQ)
				return;
			_plic.enable(1, irq);
		}

		void mask(unsigned irq)
		{
			if (irq > Global_interrupt_controller::NR_OF_IRQ)
				return;
			_plic.enable(0, irq);
		}

		void irq_mode(unsigned irq, unsigned trigger, unsigned)
		{
			using namespace Genode;

			if (irq > Global_interrupt_controller::NR_OF_IRQ ||
			    trigger == Irq_session::TRIGGER_UNCHANGED)
				return;

			_plic.el(trigger == Irq_session::TRIGGER_EDGE ? 1 : 0, irq);
		}
};

#endif /* _CORE__SPEC__RISCV__PIC_H_ */
