/*
 * \brief  Driver for the i.MX53 i2c controller
 * \author Stefan Kalkowski
 * \date   2013-03-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__IMX53__I2C_H_
#define _DRIVERS__INPUT__SPEC__IMX53__I2C_H_

/* Genode includes */
#include <platform_session/device.h>
#include <timer_session/connection.h>

/* local includes */
#include "irq_handler.h"

namespace I2c
{
	class I2c;
}


class I2c::I2c : Platform::Device::Mmio<0x12>
{
	private:

		struct Address : public Register<0x0, 16>
		{
			struct Addr : Bitfield<1,7> {};
		};

		struct Freq_divider : public Register<0x4, 16> {};

		struct Control : public Register<0x8, 16>
		{
			struct Repeat_start        : Bitfield<2,1> {};
			struct Tx_ack_enable       : Bitfield<3,1> {};
			struct Tx_rx_select        : Bitfield<4,1> {};
			struct Master_slave_select : Bitfield<5,1> {};
			struct Irq_enable          : Bitfield<6,1> {};
			struct Enable              : Bitfield<7,1> {};
		};

		struct Status : public Register<0xc, 16>
		{
			struct Rcv_ack             : Bitfield<0,1> {};
			struct Irq                 : Bitfield<1,1> {};
			struct Slave_rw            : Bitfield<2,1> {};
			struct Arbitration_lost    : Bitfield<4,1> {};
			struct Busy                : Bitfield<5,1> {};
			struct Addressed_as_slave  : Bitfield<6,1> {};
			struct Data_transfer       : Bitfield<7,1> {};
		};

		struct Data : public Register<0x10, 16> { };


		class No_ack : Genode::Exception {};

		Irq_handler _irq_handler;

		void _busy() { while (!read<Status::Busy>()); }

		void _start()
		{
			/* clock enable */
			/* input root 90 is 25Mhz target is 400Khz, divide by 64 */
			write<Freq_divider>(0x2a);
			write<Status>(0);
			write<Control>(Control::Enable::bits(1));

			while (!read<Control::Enable>()) { ; }

			write<Control::Master_slave_select>(1);

			_busy();

			write<Control>(Control::Tx_rx_select::bits(1)        |
			               Control::Tx_ack_enable::bits(1)       |
			               Control::Irq_enable::bits(1)          |
			               Control::Master_slave_select::bits(1) |
			               Control::Enable::bits(1));
		}

		void _stop()
		{
			write<Control>(0);

			/* clock disable */
		}

		void _write(Genode::uint8_t value)
		{
			write<Data>(value);

			do { _irq_handler.wait(); }
			while (!read<Status::Irq>());

			write<Status::Irq>(0);
			_irq_handler.ack();

			if (read<Status::Rcv_ack>()) throw No_ack();
		}

	public:

		I2c(Genode::Env & env, Platform::Device & dev)
		: Platform::Device::Mmio<SIZE>(dev, {0}),
		  _irq_handler(env, dev)
		{
			write<Control>(0);
			write<Status>(0);
		}

		void send(Genode::uint8_t addr, const Genode::uint8_t *buf,
		          Genode::size_t num)
		{
			while (true) {
				try {
					_start();

					_write(addr << 1);
					for (Genode::size_t i = 0; i < num; i++)
						_write(buf[i]);
					_stop();
					return;
				} catch(No_ack) { }
				 _stop();
			}
		}


		void recv(Genode::uint8_t addr, Genode::uint8_t *buf,
		          Genode::size_t num)
		{
			while (true) {
				try {
					_start();

					_write((Genode::uint8_t)(addr << 1 | 1));

					write<Control::Tx_rx_select>(0);
					if (num > 1)
						write<Control::Tx_ack_enable>(0);
					read<Data>(); /* dummy read */

					for (Genode::size_t i = 0; i < num; i++) {

						do { _irq_handler.wait(); }
						while (!read<Status::Irq>());

						write<Status::Irq>(0);

						if (i == num-1) {
							write<Control::Tx_rx_select>(0);
							write<Control::Master_slave_select>(0);
							while (read<Status::Busy>()) ;
						} else if (i == num-2) {
							write<Control::Tx_ack_enable>(1);
						}

						buf[i] = (Genode::uint8_t)read<Data>();
						_irq_handler.ack();
					}

					_stop();
					return;
				} catch(No_ack) {
					Genode::log("no ack");
					 _stop();
				}
			}
		}
};

#endif /* _DRIVERS__INPUT__SPEC__IMX53__I2C_H_ */
