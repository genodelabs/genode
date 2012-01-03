/*
 * \brief  Driver for the Xilinx LogiCORE IP XPS Interrupt Controller 2.01
 * \author Martin stein
 * \date   2010-06-21
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DEVICES__XILINX_XPS_INTC_H_ 
#define _INCLUDE__DEVICES__XILINX_XPS_INTC_H_ 

#include <cpu/config.h>

namespace Xilinx {

	/**
	 * Driver for the Xilinx LogiCORE IP XPS Interrupt Controller 2.01
	 */
	class Xps_intc
	{
		public:

			typedef Cpu::uint32_t Register;
			typedef Cpu::uint8_t  Irq;

			enum {
				REGISTER_WIDTH = sizeof(Register)*Cpu::BYTE_WIDTH,
				MIN_IRQ        = Cpu::MIN_IRQ_ID,
				MAX_IRQ        = Cpu::MAX_IRQ_ID,
				INVALID_IRQ    = Cpu::INVALID_IRQ_ID,
			};

			/**
			 * Constructor argument
			 */
			struct Constr_arg
			{
				Cpu::addr_t base;

				Constr_arg(Cpu::addr_t const & b) : base(b) { }
			};

			/**
			 * Probe if IRQ ID is valid at this controller
			 */
			inline bool valid(Irq const & i);

			/**
			 * Enable propagation of all IRQ inputs
			 */
			inline void unmask();

			/**
			 * Enable propagation of all IRQ inputs
			 */
			inline void unmask(Irq const & i);

			/**
			 * Disable propagation of all IRQ inputs
			 * (anyhow the occurency of IRQ's gets noticed in ISR)
			 */
			inline void mask();

			/**
			 * Disable propagation of an IRQ input
			 * (anyhow the occurency of the IRQ's gets noticed in ISR)
			 */
			inline void mask(Irq const & i);

			/**
			 * Constructor
			 * All IRQ's are masked initially
			 */
			inline Xps_intc(Constr_arg const & ca);

			/**
			 * Destructor
			 * All IRQ's are left masked
			 */
			inline ~Xps_intc();

			/**
			 * Get the pending IRQ with
			 * the highest priority (that one with the lowest IRQ ID)
			 */
			inline Irq next_irq();

			/**
			 * Release IRQ input so it can occure again
			 * (in general IRQ source gets acknowledged thereby)
			 */
			inline void release(Irq const & i);

			/**
			 * Probe if IRQ is pending (unmasked and active)
			 */
			inline bool pending(Irq const & i);

		private:

			/**
			 * Register mapping offsets relative to the device base address
			 */
			enum {
				RISR_OFFSET = 0 * Cpu::WORD_SIZE,
				RIPR_OFFSET = 1 * Cpu::WORD_SIZE,
				RIER_OFFSET = 2 * Cpu::WORD_SIZE,
				RIAR_OFFSET = 3 * Cpu::WORD_SIZE,
				RSIE_OFFSET = 4 * Cpu::WORD_SIZE,
				RCIE_OFFSET = 5 * Cpu::WORD_SIZE,
				RIVR_OFFSET = 6 * Cpu::WORD_SIZE,
				RMER_OFFSET = 7 * Cpu::WORD_SIZE,
				RMAX_OFFSET = 8 * Cpu::WORD_SIZE,

				RMER_ME_LSHIFT  = 0,
				RMER_HIE_LSHIFT = 1
			};

			/**
			 * Short register description (no optional registers)
			 *
			 * ISR  IRQ status register, a bit in here is '1' as long as the
			 *      according IRQ-input is '1', IRQ/bit correlation: [MAX_IRQ,...,1,0]
			 * IER  IRQ unmask register, as long as a bit is '1' in IER the controller
			 *      output equals the according bit in ISR as long as MER[ME] is '1'
			 * IAR  IRQ acknowledge register, writing a '1' to a bit in IAR writes
			 *      '0' to the according bit in ISR and '0' to bit in IAR
			 * SIE  Set IRQ unmask register, writing a '1' to a bit in SIE sets the
			 *      according bit in IER to '1' and writes '0' to the bit in SIE
			 * CIE  Clear IRQ unmask register, writing a '1' to a bit in SIE sets the
			 *      according bit in IER to '0' and writes '0' to the bit in CIE
			 * MER  Master unmask register, structure: [0,...,0,HIE,ME], controller
			 *      output is '0' as long as ME is '0', HIE is '0' initally so
			 *      software IRQ mode is active writing '1' to HIE switches to
			 *      hardware IRQ mode and masks writing to HIE
			 */
			volatile Register* const _risr;
			volatile Register* const _rier;
			volatile Register* const _riar;
			volatile Register* const _rsie;
			volatile Register* const _rcie;
			volatile Register* const _rmer;
	};
}


void Xilinx::Xps_intc::unmask() { *_rsie = ~0; }


void Xilinx::Xps_intc::unmask(Irq const & i)
{
	if (!valid(i)) { return; }
	*_rsie = 1 << i;
}


void Xilinx::Xps_intc::mask() { *_rcie = ~0; }


void Xilinx::Xps_intc::mask(Irq const & i)
{
	if (!valid(i)) { return; }
	*_rcie = 1 << i;
}


bool Xilinx::Xps_intc::pending(Irq const & i)
{
	if (!valid(i)) { return false; }
	Register const pending = *_risr & *_rier;
	return pending & (1 << i);
}


bool Xilinx::Xps_intc::valid(Irq const & i)
{
	return !(i == INVALID_IRQ || i > MAX_IRQ);
}


Xilinx::Xps_intc::Xps_intc(Constr_arg const & ca) :
	_risr((Register*)(ca.base + RISR_OFFSET)),
	_rier((Register*)(ca.base + RIER_OFFSET)),
	_riar((Register*)(ca.base + RIAR_OFFSET)),
	_rsie((Register*)(ca.base + RSIE_OFFSET)),
	_rcie((Register*)(ca.base + RCIE_OFFSET)),
	_rmer((Register*)(ca.base + RMER_OFFSET))
{
	*_rmer = 1 << RMER_HIE_LSHIFT | 1 << RMER_ME_LSHIFT;
	mask();
}


Xilinx::Xps_intc::~Xps_intc()
{
	mask();
}


Xilinx::Xps_intc::Irq Xilinx::Xps_intc::next_irq()
{
	Register const pending = *_risr & *_rier;
	Register bit_mask = 1;

	for (unsigned int i=0; i<REGISTER_WIDTH; i++) {
		if (bit_mask & pending) { return i; }
		bit_mask = bit_mask << 1;
	}
	return INVALID_IRQ;
}


void Xilinx::Xps_intc::release(Irq const & i)
{
	if (!valid(i)) { return; }
	*_riar = 1 << i;
}


#endif /* _INCLUDE__DEVICES__XILINX_XPS_INTC_H_ */

