/*
 * \brief  Fiasco(.OC) KDB UART driver
 * \author Christian Prochaska
 * \date   2013-03-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KDB_UART_H_
#define _KDB_UART_H_

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/kdebug.h>
}

/* Genode includes */
#include <base/thread.h>
#include <timer_session/connection.h>

/* local includes */
#include "uart_driver.h"

using namespace Genode;


class Kdb_uart : public Uart::Driver
{
	private:

		signed char _buffered_char;
		Lock        _kdb_read_lock;

		enum { STACK_SIZE = 2*1024*sizeof(addr_t) };

		class Char_avail_checker_thread : public Thread_deprecated<STACK_SIZE>
		{
			private:

				Uart::Driver              &_uart_driver;
				Uart::Char_avail_callback &_char_avail_callback;

			public:

				Char_avail_checker_thread(Uart::Driver &uart_driver,
				                          Uart::Char_avail_callback &char_avail_callback)
				:
					Thread_deprecated<STACK_SIZE>("char_avail_handler"),
					_uart_driver(uart_driver),
					_char_avail_callback(char_avail_callback)
				{ }

				void entry()
				{
					Timer::Connection timer;

					for(;;) {
						if (_uart_driver.char_avail())
							_char_avail_callback();
						else
							timer.msleep(20);
					}
				}

		} _char_avail_checker_thread;

	public:

		/**
		 * Constructor
		 */
		Kdb_uart(Uart::Char_avail_callback &callback)
		:
			_buffered_char(-1),
			_char_avail_checker_thread(*this, callback)
		{
			_char_avail_checker_thread.start();
		}

		/***************************
		 ** UART driver interface **
		 ***************************/

		void put_char(char c)
		{
			Fiasco::outchar(c);
		}

		bool char_avail()
		{
			Lock_guard<Lock> kdb_read_lock_guard(_kdb_read_lock);

			if (_buffered_char == -1)
				_buffered_char = Fiasco::l4kd_inchar();

			return (_buffered_char != -1);
		}

		char get_char()
		{
			char result;

			if (_buffered_char != -1) {
				result = _buffered_char;
				_buffered_char = -1;
			} else
				result = 0;

			return result;
		}

		void baud_rate(int bits_per_second)
		{
		}
};

#endif /* _KDB_UART_H_ */
