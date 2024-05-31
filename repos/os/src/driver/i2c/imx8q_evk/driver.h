/*
 * \brief  Platform specific I2C's driver for imx8q_evk
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-02-08
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _I2C_DRIVER__IMX8Q_EVK_H_
#define _I2C_DRIVER__IMX8Q_EVK_H_

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/env.h>
#include <base/log.h>
#include <base/mutex.h>
#include <timer_session/connection.h>
#include <platform_session/connection.h>
#include <irq_session/client.h>

/* Local include */
#include <i2c_interface.h>
#include "mmio.h"

namespace I2c {
	using namespace Genode;
	class Driver;
}


class I2c::Driver: public I2c::Driver_base
{
	public:

		struct Args
		{
			bool        verbose;
			unsigned    bus_no;
			Device_name device_name;
			uint16_t    bus_speed_khz;
		};

	private:

		Env       &_env;
		Args const _args;

		Platform::Connection      _platform { _env };
		Platform::Device          _device   { _platform };
		I2c::Mmio                 _mmio     { _device };
		Platform::Device::Irq     _irq      { _device };
		Io_signal_handler<Driver> _irq_handler;

		unsigned          _sem_cnt = 1;
		Timer::Connection _timer { _env };

		void _bus_reset();
		void _bus_start();
		void _bus_stop();
		void _bus_write(Genode::uint8_t data);
		void _bus_busy();

		void _wait_for_irq();
		void _irq_handle() { _sem_cnt = 0; }

		void _write(uint8_t, I2c::Session::Message&);
		void _read(uint8_t, I2c::Session::Message&);

	public:

		Driver(Env &env, Args const &args)
		:
			_env(env), _args(args),
			_irq_handler(_env.ep(), *this, &Driver::_irq_handle)
		{
			_irq.sigh(_irq_handler);
			_irq_handle();
			_irq.ack();
			_bus_reset();
		}

		void transmit(uint8_t address, I2c::Session::Transaction & t) override;
};

#endif /* _I2C_DRIVER__IMX8Q_EVK_H_ */
