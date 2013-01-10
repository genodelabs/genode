/*
 * \brief  Interface to i8042 controller
 * \author Norman Feske
 * \date   2007-09-21
 *
 * This is a simplified version for DDE kit test. The original resides in the
 * os repository under src/drivers/input/ps2.
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _I8042_H_
#define _I8042_H_

#include <io_port_session/connection.h>
#include <base/env.h>


class I8042
{
	enum Register
	{
		REG_DATA         = 0x60,
		REG_STATUS       = 0x64,
		REG_COMMAND      = 0x64,
	};

	enum Flag
	{
		/* status register */
		STAT_OBF         = 0x01,
		STAT_IBF         = 0x02,

		/* control register */
		CTRL_KBD_INT     = 0x01,
		CTRL_AUX_INT     = 0x02,
		CTRL_XLATE       = 0x40,
	};

	enum Command
	{
		CMD_READ         = 0x20,
		CMD_WRITE        = 0x60,
		CMD_TEST         = 0xaa,

		CMD_AUX_ENABLE   = 0xa8,
		CMD_AUX_TEST     = 0xa9,

		CMD_KBD_ENABLE   = 0xae,
		CMD_KBD_TEST     = 0xab,

		CMD_AUX_WRITE    = 0xd4,
	};

	enum Return
	{
		RET_TEST_OK      = 0x55,
		RET_KBD_TEST_OK  = 0x00,
		RET_AUX_TEST_OK  = 0x00,
	};

	private:

		Genode::Io_port_connection _data_port;  /* data port */
		Genode::Io_port_connection _stat_port;  /* status/command port */

		/**
		 * Read controller status
		 */
		unsigned char _status() { return _stat_port.inb(REG_STATUS); }

		/**
		 * Read data from controller
		 */
		unsigned char _data() { return _data_port.inb(REG_DATA); }

		/**
		 * Issue command to controller
		 */
		void _command(unsigned char cmd)
		{
			while (_input_buffer_full());
			_stat_port.outb(REG_STATUS, cmd);
		}

		/**
		 * Send data to controller
		 */
		void _data(unsigned char value)
		{
			while (_input_buffer_full());
			_data_port.outb(REG_DATA, value);
		}

		/**
		 * Convenience functions for accessing the controller status
		 */
		bool _output_buffer_full() { return _status() & STAT_OBF; }
		bool _input_buffer_full()  { return _status() & STAT_IBF; }

		/**
		 * Wait for data and read
		 */
		unsigned char _wait_data()
		{
			while (!_output_buffer_full());
			return _data();
		}

	public:

		/**
		 * Constructor
		 */
		I8042() : _data_port(REG_DATA, 1), _stat_port(REG_STATUS, 1) { reset(); }

		/**
		 * Test and initialize controller
		 */
		void reset()
		{
			int ret = 0;

			/* read remaining data in controller */
			while (_output_buffer_full()) _data();

			/* run self tests */
			_command(CMD_TEST);
			if ((ret = _wait_data()) != RET_TEST_OK) {
				Genode::printf("i8042: self test failed (%x)\n", ret);
				return;
			}
			_command(CMD_KBD_TEST);
			if ((ret = _wait_data()) != RET_KBD_TEST_OK) {
				Genode::printf("i8042: kbd test failed (%x)\n", ret);
				return;
			}
			_command(CMD_AUX_TEST);
			if ((ret = _wait_data()) != RET_AUX_TEST_OK) {
				Genode::printf("i8042: aux test failed (%x)\n", ret);
				return;
			}

			/* enable keyboard and mouse */
			_command(CMD_READ);
			ret  = _wait_data();
			ret &= ~CTRL_XLATE;
			ret |= CTRL_KBD_INT | CTRL_AUX_INT;
			_command(CMD_WRITE);
			_data(ret);
			_command(CMD_KBD_ENABLE);
			_command(CMD_AUX_ENABLE);

			/* initialize keyboard */
			_data(0xf0);
			if (_data() != 0xfa) {
				PWRN("Scan code setting not supported");
			} else {
				_data(2);
				if (_data() != 0xfa)
					PWRN("Scan code 2 not supported");
			}

			/* initialize mouse */
			_command(CMD_AUX_WRITE);
			_data(0xf6);
			if (_data() != 0xfa)
				PWRN("Could not set defaults");
			_command(CMD_AUX_WRITE);
			_data(0xf4);
			if (_data() != 0xfa)
				PWRN("Could not enable stream");
		}

		/**
		 * Flush data from controller
		 */
		void flush() { while (_output_buffer_full()) _data(); }
};

#endif
