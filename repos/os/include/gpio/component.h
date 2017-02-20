/*
 * \brief  GPIO-session component
 * \author Stefan Kalkowski
 * \date   2013-05-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

		struct Irq_session_component : public Genode::Rpc_object<Genode::Irq_session>
		{
			Driver        &_driver;
			unsigned long  _pin;

			Irq_session_component(Driver &driver, unsigned long pin)
			: _driver(driver), _pin(pin) { }


			/***************************
			 ** Irq_session interface **
			 ***************************/

			void ack_irq() override { _driver.ack_irq(_pin); }
			void sigh(Genode::Signal_context_capability sigh) override {
				_driver.register_signal(_pin, sigh); }
			Info info() override {
				return { .type = Genode::Irq_session::Info::Type::INVALID }; }
		};

		Genode::Rpc_entrypoint &_ep;
		Driver                 &_driver;
		unsigned long           _pin;

		Irq_session_component          _irq_component;
		Genode::Irq_session_capability _irq_cap;


	public:

		Session_component(Genode::Rpc_entrypoint &ep,
		                  Driver                 &driver,
		                  unsigned long           gpio_pin)
		: _ep(ep), _driver(driver), _pin(gpio_pin),
		  _irq_component(_driver, _pin),
		  _irq_cap(_ep.manage(&_irq_component)) { }

		~Session_component() { _ep.dissolve(&_irq_component); }


		/*****************************
		 ** Gpio::Session interface **
		 *****************************/

		void direction(Direction d)  { _driver.direction(_pin, (d == IN)); }
		void write(bool level)       { _driver.write(_pin, level);         }
		bool read()                  { return _driver.read(_pin);          }

		void debouncing(unsigned int us)
		{
			if (us) {
				_driver.debounce_time(_pin, us);
				_driver.debounce_enable(_pin, true);
			} else
				_driver.debounce_enable(_pin, false);
		}

		Genode::Irq_session_capability irq_session(Irq_type type)
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

			_driver.irq_enable(_pin, true);

			return _irq_cap;
		}
};


class Gpio::Root : public Genode::Root_component<Gpio::Session_component>
{
	private:

		Genode::Rpc_entrypoint &_ep;
		Driver                 &_driver;

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
				Genode::warning("insufficient dontated ram_quota "
				                "(", ram_quota, " bytes), "
				                "require ", sizeof(Session_component), " bytes");
				throw Genode::Root::Quota_exceeded();
			}

			return new (md_alloc()) Session_component(_ep, _driver, pin);
		}

	public:

		Root(Genode::Rpc_entrypoint *session_ep,
		     Genode::Allocator *md_alloc, Driver &driver)
		: Genode::Root_component<Gpio::Session_component>(session_ep, md_alloc),
		  _ep(*session_ep), _driver(driver) { }
};


#endif /* _INCLUDE__GPIO__COMPONENT_H_ */
