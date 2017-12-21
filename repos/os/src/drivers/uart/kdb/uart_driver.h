/*
 * \brief  Fiasco(.OC) KDB UART driver
 * \author Christian Prochaska
 * \date   2013-03-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KDB_UART_H_
#define _KDB_UART_H_

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/kdebug.h>
}

/* Genode includes */
#include <base/signal.h>
#include <timer_session/connection.h>

using namespace Genode;

namespace Uart {
	class Driver;
	struct Driver_factory;
	struct Char_avail_functor;
};


/**
 * Functor, called by 'Driver' when data is ready for reading
 */
struct Uart::Char_avail_functor
{
	Genode::Signal_context_capability sigh;

	void operator ()() {
		if (sigh.valid()) Genode::Signal_transmitter(sigh).submit();
		else Genode::error("no sigh"); }

};



class Uart::Driver
{
	private:

		signed char            _buffered_char;
		Char_avail_functor    &_char_avail;
		Timer::Connection      _timer;
		Signal_handler<Driver> _timer_handler;

		void _timeout() { if (char_avail()) _char_avail(); }

	public:

		Driver(Genode::Env &env, unsigned, unsigned,
		       Uart::Char_avail_functor &func)
		: _buffered_char(-1),
		  _char_avail(func),
		  _timer(env),
		  _timer_handler(env.ep(), *this, &Driver::_timeout)
		{
			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(20*1000);
		}


		/***************************
		 ** UART driver interface **
		 ***************************/

		void put_char(char c) { Fiasco::outchar(c); }

		bool char_avail()
		{
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

		void baud_rate(int) { }
};


/**
 * Factory used by 'Uart::Root' at session creation/destruction time
 */
struct Uart::Driver_factory
{
	struct Not_available {};

	enum { UARTS_NUM = 1 };

	Genode::Env  &env;
	Genode::Heap &heap;
	Driver       *drivers[UARTS_NUM] { nullptr };

	Driver_factory(Genode::Env &env, Genode::Heap &heap)
	: env(env), heap(heap) {}


	Uart::Driver &create(unsigned index, unsigned baudrate,
	                     Uart::Char_avail_functor &callback);

	void destroy(Uart::Driver *) { /* TODO */ }

};

#endif /* _KDB_UART_H_ */
