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

/* platform includes */
#include <pl050_defs.h>

/* Genode includes */
#include <os/ring_buffer.h>
#include <io_mem_session/connection.h>

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


	class _Channel : public Serial_interface,
	                 public Genode::Ring_buffer<unsigned char, 256>
	{
		private:

			Genode::Lock               _lock;
			Genode::Io_mem_connection  _io_mem;
			volatile Genode::uint32_t *_reg_base;

			/**
			 * Attach memory-mapped PL050 registers to local address space
			 *
			 * \return local base address of memory-mapped I/O registers
			 */
			Genode::uint32_t *_attach(Genode::Io_mem_session &ios)
			{
				return Genode::env()->rm_session()->attach(ios.dataspace());
			}

			/**
			 * Return true if input is available
			 */
			bool _input_pending() {
				return _reg_base[PL050_REG_IIR] & PL050_IIR_RX_INTR; }

		public:

			/**
			 * Constructor
			 *
			 * \param phys_base  local address of the channel's device
			 *                   registers
			 */
			_Channel(Genode::addr_t phys_base, Genode::size_t phys_size)
			:
				_io_mem(phys_base, phys_size), _reg_base(_attach(_io_mem))
			{ }

			/**
			 * Read input or wait busily until input becomes available
			 */
			unsigned char read()
			{
				Genode::Lock::Guard guard(_lock);

				while (empty())
					if (_input_pending())
						add(_reg_base[PL050_REG_DATA]);

				return get();
			}

			void write(unsigned char value)
			{
				while (!(_reg_base[PL050_REG_STATUS] & PL050_STATUS_TX_EMPTY));
				_reg_base[PL050_REG_DATA] = value;
			}

			bool data_read_ready()
			{
				return !empty() || _input_pending();
			}

			void enable_irq()
			{
				_reg_base[PL050_REG_CONTROL] = PL050_CONTROL_RX_IRQ | PL050_CONTROL_ENABLE;
			}
	};

	private:

		_Channel _kbd;
		_Channel _aux;

	public:

		/**
		 * Constructor
		 */
		Pl050() :
			_kbd(PL050_KEYBD_PHYS, PL050_KEYBD_SIZE),
			_aux(PL050_MOUSE_PHYS, PL050_MOUSE_SIZE)
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
