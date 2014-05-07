/*
 * \brief  GPIO-session component
 * \author Stefan Kalkowski
 * \date   2013-05-03
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GPIO__COMPONENT_H_
#define _INCLUDE__GPIO__COMPONENT_H_

#include <base/printf.h>
#include <root/component.h>
#include <gpio_session/gpio_session.h>
#include <gpio/driver.h>

namespace Gpio {
	class Session_component;
	class Root;
};


class Gpio::Session_component : public Genode::Rpc_object<Gpio::Session>
{
	private:

		Driver                           &_driver;
		unsigned long                     _pin;
		Genode::Signal_context_capability _sigh;

	public:

		Session_component(Driver &driver, unsigned long gpio_pin)
		: _driver(driver), _pin(gpio_pin) { }

		~Session_component()
		{
			if (_sigh.valid())
				_driver.unregister_signal(_pin);
		}

		/************************************
		 ** Gpio::Session interface **
		 ************************************/

		void direction(Direction d)  { _driver.direction(_pin, (d == IN)); }
		void write(bool level)       { _driver.write(_pin, level);         }
		bool read()                  { return _driver.read(_pin);          }
		void irq_enable(bool enable) { _driver.irq_enable(_pin, enable);   }

		void irq_sigh(Genode::Signal_context_capability cap)
		{
			if (cap.valid()) {
				_sigh = cap;
				_driver.register_signal(_pin, cap);
			}
		}

		void irq_type(Irq_type type)
		{
			switch (type) {
			case HIGH_LEVEL:
				_driver.high_detect(_pin);
				break;
			case LOW_LEVEL:
				_driver.low_detect(_pin);
				break;
			case RISING_EDGE:
				_driver.rising_detect(_pin);
				break;
			case FALLING_EDGE:
				_driver.falling_detect(_pin);
			};
		}

		void debouncing(unsigned int us)
		{
			if (us) {
				_driver.debounce_time(_pin, us);
				_driver.debounce_enable(_pin, true);
			} else
				_driver.debounce_enable(_pin, false);
		}
};


class Gpio::Root : public Genode::Root_component<Gpio::Session_component>
{
	private:

		Driver &_driver;

	protected:

		Session_component *_create_session(const char *args)
		{
			unsigned long pin =
				Genode::Arg_string::find_arg(args, "gpio").ulong_value(0);
			Genode::size_t ram_quota  =
				Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			if (!_driver.gpio_valid(pin))
				throw Invalid_args();

			if (ram_quota < sizeof(Session_component)) {
				PWRN("Insufficient dontated ram_quota (%zd bytes), require %zd bytes",
					 ram_quota, sizeof(Session_component));
				throw Genode::Root::Quota_exceeded();
			}

			return new (md_alloc()) Session_component(_driver, pin);
		}

	public:

		Root(Genode::Rpc_entrypoint *session_ep,
		     Genode::Allocator *md_alloc, Driver &driver)
		: Genode::Root_component<Gpio::Session_component>(session_ep, md_alloc),
		  _driver(driver) { }
};


#endif /* _INCLUDE__GPIO__COMPONENT_H_ */
