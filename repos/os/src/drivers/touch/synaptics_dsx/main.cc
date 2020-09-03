/**
 * \brief  Synaptics DSX touch input
 * \author Sebastian Sumpf
 * \date   2020-09-03
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/component.h>
#include <base/env.h>

#include <event_session/connection.h>
#include <gpio_session/connection.h>
#include <platform_session/connection.h>
#include <i2c.h>

using namespace Genode;


struct Finger_data
{
	uint8_t status;
	uint8_t x_lsb;
	uint8_t x_msb;
	uint8_t y_lsb;
	uint8_t y_msb;
	uint8_t wx;
	uint8_t wy;

	unsigned x() const
	{
		return 1080 - ((x_msb << 8) | x_lsb);
	}

	unsigned y() const
	{
		return 1920 - ((y_msb << 8) | y_lsb);
	}

	void dump() const
	{
		log("status: ", Hex(status),
		    " x_lsb: ", x_lsb, " x_msb: ", x_msb,
		    " y_lsb: ", y_lsb, " y_msb: ", y_msb,
		    " wx: ", wx, " wy: ", wy,
		    " x: ", x(),
		    " y: ", y());
	}
};


struct Synaptics
{
	enum { FINGERS = 5, I2C_ADDR = 0x20,  };
	enum Gpio_irq { IRQ = 135 };

	Genode::Env &env;
	Irq_handler                  _i2c_irq_handler;
	Attached_dataspace           _i2c_ds;
	I2c::I2c                     _i2c { (addr_t)_i2c_ds.local_addr<addr_t>(), _i2c_irq_handler };
	Gpio::Connection             _gpio { env, IRQ };
	Irq_session_client           _irq { _gpio.irq_session(Gpio::Session::LOW_LEVEL) };
	Io_signal_handler<Synaptics> _irq_dispatcher { env.ep(), *this, &Synaptics::_handle_irq };
	Event::Connection            _event { env };
	uint8_t                      _buf[10];
	bool                         _button[FINGERS] { };

	void _handle_event(Event::Session_client::Batch &batch)
	{
		/* retrieve status */
		Finger_data fingers[FINGERS];
		_buf[0] = 6;
		_i2c.send(I2C_ADDR, _buf, 1);
		_i2c.recv(I2C_ADDR, (uint8_t *)fingers, sizeof(fingers));

		for (int i = 0; i < FINGERS; i++) {
			Finger_data &current = fingers[i];

			Input::Touch_id id { i };

			if (current.status == 0) {
				if (_button[i]) {
					batch.submit(Input::Release{Input::BTN_LEFT});
					batch.submit(Input::Touch_release{id});
					_button[i] = false;
				}
				continue;
			}

			batch.submit(Input::Absolute_motion { (int)current.x(), (int)current.y() });
			batch.submit(Input::Touch { id, (float)current.x(), (float)current.y() });

			if (_button[i] == false) {
				batch.submit(Input::Press { Input::BTN_LEFT });
			}
			_button[i] = true;
		}
	}

	void _handle_irq()
	{
		/* read device IRQ */
		_buf[0] = 4;
		_i2c.send(I2C_ADDR, _buf, 1);
		_i2c.recv(I2C_ADDR, _buf, 2);

		_event.with_batch([&] (Event::Session_client::Batch &batch) {
			_handle_event(batch);
		});

		_irq.ack_irq();
	}

	Synaptics(Env &env, Dataspace_capability io_mem,
	          Irq_session_capability irq)
	: env(env),
	 _i2c_irq_handler(env, irq),
	 _i2c_ds(env.rm(), io_mem)
	{

		/* set page 0 */
		_buf[0] = 0xff;
		_buf[1] = 0;
		_i2c.send(I2C_ADDR, _buf, 2);

		/* enable interrupt */
		_buf[0] = 0xf;
		_buf[1] = 0x16;
		_i2c.send(I2C_ADDR, _buf, 2);

		/* set configured */
		_buf[0] = 0xe;
		_buf[1] = 0x84;
		_i2c.send(I2C_ADDR, _buf, 2);

		/* GPIO touchscreen handling */
		_gpio.direction(Gpio::Session::IN);

		_irq.sigh(_irq_dispatcher);
		_irq.ack_irq();
	}
};


struct Main
{
	Platform::Connection     _platform_connection;
	Constructible<Synaptics> _synaptics { };

	Main(Env &env)
	: _platform_connection(env)
	{
		using namespace Platform;

		Device_capability cap;
		try {
			cap = _platform_connection.device_by_index(0);
		} catch (...) {
			error("Could not acquire device resources");
			return;
		}

		if (cap.valid() == false) {
			return;
		}

		Device_client device { cap };

		Dataspace_capability io_mem  = device.io_mem_dataspace();
		if (io_mem.valid() == false) {
			Genode::warning("No 'io_mem' node present ... skipping");
			return;
		}

		Irq_session_capability irq = device.irq();
		if (irq.valid() == false) {
			Genode::warning("No 'irq' node present ... skipping");
			return;
		}

		_synaptics.construct(env, io_mem, irq);
	}
};

void Component::construct(Genode::Env &env) { static Main main(env); }
