/*
 * \brief  PL050 PS/2 controller driver
 * \author Norman Feske
 * \date   2010-03-23
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__PS2__PL050__PL050_H_
#define _DRIVERS__INPUT__SPEC__PS2__PL050__PL050_H_

/* Genode includes */
#include <os/ring_buffer.h>
#include <base/attached_dataspace.h>
#include <io_mem_session/client.h>

/* local includes */
#include "serial_interface.h"


class Pl050
{
	/*
	 * Device definitions
	 */
	enum {
		/**
		 * Register offsets (in 32-bit words)
		 */
		PL050_REG_CONTROL = 0,  /* control */
		PL050_REG_STATUS  = 1,  /* status */
		PL050_REG_DATA    = 2,  /* data */
		PL050_REG_IIR     = 4,  /* irq control */

		/**
		 * Bit definitions of control register
		 */
		PL050_CONTROL_ENABLE = (1 << 2),
		PL050_CONTROL_RX_IRQ = (1 << 4),

		/**
		 * Bit definitions of status register
		 */
		PL050_STATUS_RX_FULL  = (1 << 4),
		PL050_STATUS_TX_EMPTY = (1 << 6),

		/**
		 * Bit definitions of interrupt control register
		 */
		PL050_IIR_RX_INTR = (1 << 0),
	};


	class _Channel : public  Serial_interface,
	                 private Genode::Ring_buffer<unsigned char, 256>
	{
		private:

			/*
			 * Noncopyable
			 */
			_Channel(_Channel const &);
			_Channel &operator = (_Channel const &);

			volatile Genode::uint32_t * _reg_base;

			/**
			 * Return true if input is available
			 */
			bool _input_pending() {
				return _reg_base[PL050_REG_IIR] & PL050_IIR_RX_INTR; }

		public:

			_Channel(Platform::Device::Mmio<0> &mmio)
			:
				_reg_base(mmio.local_addr<Genode::uint32_t>())
			{ }

			/**
			 * Read input or wait busily until input becomes available
			 */
			unsigned char read() override
			{
				while (empty())
					if (_input_pending())
						add((Genode::uint8_t)_reg_base[PL050_REG_DATA]);

				return get();
			}

			void write(unsigned char value) override
			{
				while (!(_reg_base[PL050_REG_STATUS] & PL050_STATUS_TX_EMPTY));

				_reg_base[PL050_REG_DATA] = value;
			}

			bool data_read_ready() override
			{
				return !empty() || _input_pending();
			}

			void enable_irq() override
			{
				_reg_base[PL050_REG_CONTROL] = PL050_CONTROL_RX_IRQ | PL050_CONTROL_ENABLE;
			}
	};

	private:

		_Channel _kbd;
		_Channel _aux;

	public:

		Pl050(Platform::Device::Mmio<0> &keyboard_mmio,
		      Platform::Device::Mmio<0> &mouse_mmio)
		:
			_kbd(keyboard_mmio), _aux(mouse_mmio)
		{
			_kbd.enable_irq();
			_aux.enable_irq();
		}

		/**
		 * Request serial keyboard interface
		 */
		Serial_interface &kbd_interface() { return _kbd; }

		/**
		 * Request serial mouse interface
		 */
		Serial_interface &aux_interface() { return _aux; }
};

#endif /* _DRIVERS__INPUT__SPEC__PS2__PL050__PL050_H_ */
