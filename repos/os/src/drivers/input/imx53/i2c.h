/*
 * \brief  Driver for the i.MX53 i2c controller
 * \author Stefan Kalkowski
 * \date   2013-03-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _I2C_H_
#define _I2C_H_

/* Genode includes */
#include <util/mmio.h>
#include <timer_session/connection.h>
#include <irq_session/connection.h>

namespace I2c
{
	class I2c;
}


class I2c::I2c : Genode::Mmio
{
	private:

		struct Address : public Register<0x0, 8>
		{
			struct Addr : Bitfield<1,7> {};
		};

		struct Freq_divider : public Register<0x4, 8> {};

		struct Control : public Register<0x8, 8>
		{
			struct Repeat_start        : Bitfield<2,1> {};
			struct Tx_ack_enable       : Bitfield<3,1> {};
			struct Tx_rx_select        : Bitfield<4,1> {};
			struct Master_slave_select : Bitfield<5,1> {};
			struct Irq_enable          : Bitfield<6,1> {};
			struct Enable              : Bitfield<7,1> {};
		};

		struct Status : public Register<0xc, 8>
		{
			struct Rcv_ack             : Bitfield<0,1> {};
			struct Irq                 : Bitfield<1,1> {};
			struct Slave_rw            : Bitfield<2,1> {};
			struct Arbitration_lost    : Bitfield<4,1> {};
			struct Busy                : Bitfield<5,1> {};
			struct Addressed_as_slave  : Bitfield<6,1> {};
			struct Data_transfer       : Bitfield<7,1> {};
		};

		struct Data : public Register<0x10, 8> { };


		class No_ack : Genode::Exception {};
		class No_irq : Genode::Exception {};


		Timer::Connection      _timer;
		Genode::Irq_connection _irq;

		void _busy() { while (!read<Status::Busy>()); }

		void _start()
		{
			/* clock enable */

			write<Freq_divider>(0x2c);
			write<Status>(0);
			write<Control>(Control::Enable::bits(1));

			_timer.msleep(1);

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

			_irq.wait_for_irq();
			if (!read<Status::Irq>()) throw No_irq();
			write<Status::Irq>(0);
			if (read<Status::Rcv_ack>()) throw No_ack();
		}

	public:

		I2c(Genode::addr_t const base, unsigned irq) : Mmio(base), _irq(irq)
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
				} catch(No_ack) {
				} catch(No_irq) {
				}
				 _stop();
			}
		}


		void recv(Genode::uint8_t addr, Genode::uint8_t *buf,
		          Genode::size_t num)
		{
			while (true) {
				try {
					_start();
					_write(addr << 1 | 1);
					write<Control::Tx_rx_select>(0);
					if (num > 1)
						write<Control::Tx_ack_enable>(0);
					read<Data>(); /* dummy read */

					for (Genode::size_t i = 0; i < num; i++) {

						_irq.wait_for_irq();
						if (!read<Status::Irq>()) throw No_irq();
						write<Status::Irq>(0);

						if (i == num-1) {
							write<Control::Tx_rx_select>(0);
							write<Control::Master_slave_select>(0);
							while (read<Status::Busy>()) ;
						} else if (i == num-2) {
							write<Control::Tx_ack_enable>(1);
						}

						buf[i] = read<Data>();
					}

					_stop();
					return;
				} catch(No_irq) {
				}
				 _stop();
			}
		}

};

#endif /* _I2C_H_ */
