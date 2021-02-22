/*
 * \brief  Platform specific i2c's driver for imx8q_evk
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

/* Local include */
#include <i2c_interface.h>
#include "mmio.h"

namespace I2c {
	using namespace Genode;
	class Driver;
}

class I2c::Driver: public I2c::Driver_base
{
	private:

		Platform::Connection                      _platform_connection { _env };
		Constructible<I2c::Mmio>                  _mmio {};
		Constructible<Genode::Attached_dataspace> _i2c_ctl_ds {};
		Constructible<Genode::Irq_session_client> _irq {};
		Io_signal_handler<Driver>                 _irq_handler;
		volatile unsigned                         _sem_cnt = 1;
		Mutex                                     _bus_mutex {};
		Timer::Connection                         _timer { _env };

		bool          _is_verbose = false;
		Genode::off_t _bus_no = 0;

		void _init_driver();
		void _bus_reset();
		void _bus_start();
		void _bus_stop();
		void _bus_write(Genode::uint8_t data);
		void _bus_busy();

		void _wait_for_irq();
		void _irq_handle() { _sem_cnt = 0; }

	public:

		Driver(Env            &env,
		       Xml_node const &config)
		:
			Driver_base(env, config),
			_irq_handler(_env.ep(), *this, &Driver::_irq_handle)
		{
			_init_driver();
			_irq->sigh(_irq_handler);
			_irq->ack_irq();
		}

		virtual ~Driver() = default;

		void write(uint8_t address, const uint8_t *buffer_in, const size_t buffer_size) override;
		void read(uint8_t address, uint8_t *buffer_out, const size_t buffer_size) override;
};

#endif /* _I2C_DRIVER__IMX8Q_EVK_H_ */
