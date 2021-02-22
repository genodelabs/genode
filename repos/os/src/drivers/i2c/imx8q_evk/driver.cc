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

#include <i2c_session/i2c_session.h>
#include "driver.h"


void I2c::Driver::_wait_for_irq()
{
	_sem_cnt++;
	while (_sem_cnt > 0)
		_env.ep().wait_and_dispatch_one_io_signal();

	if (_mmio.read<Mmio::Control::Master_slave_select>() == 0) {
		_bus_stop();
		if (_args.verbose) {
			error("Arbitrationtion lost on bus ", _args.bus_no);
		}
		throw I2c::Session::Bus_error();
	}
}


void I2c::Driver::_bus_busy()
{
	uint64_t const start_time = _timer.elapsed_ms();
	while (!_mmio.read<Mmio::Status::Busy>()) {
		uint64_t const current = _timer.elapsed_ms();
		if (current - start_time > 1000) {
			_bus_stop();
			if (_args.verbose) {
				error("Timeout on bus ", _args.bus_no);
			}
			throw I2c::Session::Bus_error();
		}
	}
}


void I2c::Driver::_bus_reset()
{
	_mmio.write<Mmio::Control>(0);
	_mmio.write<Mmio::Status>(0);
}


void I2c::Driver::_bus_start()
{
	_bus_reset();

	/* input root 90 is 25Mhz target is 400Khz, divide by 64 */
	_mmio.write<Mmio::Freq_divider>(0x2a);
	_mmio.write<Mmio::Status>(0);
	_mmio.write<Mmio::Control>(Mmio::Control::Enable::bits(1));

	uint64_t const start_time = _timer.elapsed_ms();
	while (!_mmio.read<Mmio::Control::Enable>()) {
		uint64_t const current = _timer.elapsed_ms();
		if (current - start_time > 1000) {
			_bus_stop();
			if (_args.verbose) {
				error("Timeout on bus ", _args.bus_no);
			}
			throw I2c::Session::Bus_error();
		}
	}

	_mmio.write<Mmio::Control::Master_slave_select>(1);

	_bus_busy();

	_mmio.write<Mmio::Control>(Mmio::Control::Tx_rx_select::bits(1)        |
	                           Mmio::Control::Tx_ack_enable::bits(1)       |
	                           Mmio::Control::Irq_enable::bits(1)          |
	                           Mmio::Control::Master_slave_select::bits(1) |
	                           Mmio::Control::Enable::bits(1));

	_mmio.write<Mmio::Status::Ial>(0);
}


void I2c::Driver::_bus_stop()
{
	_mmio.write<Mmio::Control>(0);
}


void I2c::Driver::_bus_write(uint8_t data)
{
	_mmio.write<Mmio::Data>(data);

	do { _wait_for_irq(); }
	while (!_mmio.read<Mmio::Status::Irq>());

	_mmio.write<Mmio::Status::Irq>(0);
	_irq.ack_irq();

	if (_mmio.read<Mmio::Status::Rcv_ack>()) {
		_bus_stop();
		if (_args.verbose) {
			error("Slave did not acknoledge on bus ", _args.bus_no);
		}
		throw I2c::Session::Bus_error();
	}
}


void I2c::Driver::write(uint8_t address, uint8_t const *buffer_in, size_t const buffer_size)
{
	_bus_start();

	/* LSB must be 0 for writing on the bus, address is on the 7 hightest bits */
	_bus_write(address << 1);
	for (size_t idx = 0; idx < buffer_size; ++idx) {
		_bus_write(buffer_in[idx]);
	}
	_bus_stop();
}


void I2c::Driver::read(uint8_t address, uint8_t *buffer_out, size_t const buffer_size)
{
	_bus_start();

	/* LSB must be 1 for reading on the bus, address is on the 7 hightest bits */
	_bus_write(address << 1 | 1);

	_mmio.write<Mmio::Control::Tx_rx_select>(0);
	if (buffer_size > 1) {
		_mmio.write<Mmio::Control::Tx_ack_enable>(0);
	}
	_mmio.read<Mmio::Data>();

	for (size_t i = 0; i < buffer_size; ++i) {

		do { _wait_for_irq(); }
		while (!_mmio.read<Mmio::Status::Irq>());

		_mmio.write<Mmio::Status::Irq>(0);

		if (i == buffer_size - 1) {
			_mmio.write<Mmio::Control::Tx_rx_select>(0);
			_mmio.write<Mmio::Control::Master_slave_select>(0);
			while (_mmio.read<Mmio::Status::Busy>());
		} else if (i == buffer_size - 2) {
			_mmio.write<Mmio::Control::Tx_ack_enable>(1);
		}

		buffer_out[i] = _mmio.read<Mmio::Data>();
		_irq.ack_irq();
	}

	_bus_stop();
}
