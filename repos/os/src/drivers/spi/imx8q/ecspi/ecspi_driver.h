/*
 * \brief  Ecspi driver for imx8q
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-04-20
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IMX8Q_ECSPI__DRIVER_H_
#define _INCLUDE__IMX8Q_ECSPI__DRIVER_H_

/* Genode includes */
#include <base/env.h>
#include <base/semaphore.h>
#include <irq_session/client.h>
#include <os/ring_buffer.h>
#include <platform_session/connection.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>

/* Local includes */
#include "spi_driver.h"
#include "ecspi_mmio.h"

namespace Spi {
	using namespace Genode;

	class Ecspi_driver;
}


class Spi::Ecspi_driver : public Spi::Driver {
public:

	struct Config {
		bool     verbose       = false;
		bool     loopback      = false;
		uint8_t  clock_divider = 0;
		uint64_t timeout       = 1000; /* in millisecond */
	};


private:

	Genode::Env      &_env;
	const Config      _config;
	Timer::Connection _timer { _env };

	/* platform connection */
	Platform::Connection    _platform_connection{ _env };
	Platform::Device_client _device{ _platform_connection.device_by_index(0) };

	/* iomem */
	Attached_dataspace _spi_ctl_ds{ _env.rm(), _device.io_mem_dataspace(0) };
	Spi::Mmio          _mmio{ reinterpret_cast<addr_t>(_spi_ctl_ds.local_addr<uint16_t>()) };

	/* interrupt handler */
	Genode::Semaphore               _sem_exchange { 1 };
	Irq_session_client              _irq{ _device.irq() };
	Io_signal_handler<Ecspi_driver> _irq_handler;

	void _irq_handle();

	struct Transfer {

		static constexpr size_t MAX_BURST_SIZE = 256;

		uint8_t *buffer      = nullptr;
		size_t   buffer_size = 0;
		size_t   tx_bytes    = 0;
		size_t   rx_bytes    = 0;
	};

	void _bus_enable(Settings const &settings, size_t slave_select);

	void _bus_disable();

	void _bus_exchange(Transfer &transaction);

	void _bus_execute_transaction();

	size_t _fifo_write(uint8_t const *buffer, size_t size);

	size_t _fifo_read_unaligned(uint8_t *buffer, size_t size);

	size_t _fifo_read(uint8_t *buffer, size_t size);


public:

	explicit Ecspi_driver(Ecspi_driver const&)          = delete;
	Ecspi_driver const &operator=(Ecspi_driver const &) = delete;

	Ecspi_driver(Genode::Env &env, Config const &config)
	:
		_env { env },
		_config { config },
		_irq_handler { _env.ep(), *this, &Ecspi_driver::_irq_handle }
	{
		_irq.sigh(_irq_handler);
		_irq_handle();
		_irq.ack_irq();
	}

	size_t transfer(Transaction& trxn) override;

	char const *name() const override { return "ECSPI"; }
};

#endif // _INCLUDE__IMX8Q_ECSPI__DRIVER_H_
