/*
 * \brief  UART LOG component
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UART_COMPONENT_H_
#define _UART_COMPONENT_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/rpc_server.h>
#include <util/arg_string.h>
#include <os/session_policy.h>
#include <base/component.h>
#include <root/component.h>
#include <uart_session/uart_session.h>

/* local includes */
#include <uart_driver.h>

namespace Uart {

	using namespace Genode;

	class Session_component;
	class Root;

	typedef Root_component<Session_component, Multiple_clients> Root_component;
};


class Uart::Session_component : public Rpc_object<Uart::Session,
                                                  Uart::Session_component>
{
	private:

		/*
		 * XXX Do not use hard-coded value, better make it dependent
		 *     on the RAM quota donated by the client.
		 */
		enum { IO_BUFFER_SIZE = 4096 };

		Genode::Attached_ram_dataspace _io_buffer;

		/**
		 * Functor informing the client about new data to read
		 */
		Char_avail_functor    _char_avail { };
		Uart::Driver_factory &_driver_factory;
		Uart::Driver         &_driver;

		Size _size;

		unsigned char _poll_char()
		{
			while (!_driver.char_avail());
			return _driver.get_char();
		}

		void _put_string(char const *s)
		{
			for (; *s; s++)
				_driver.put_char(*s);
		}

		/**
		 * Read ASCII number from UART
		 *
		 * \return character that terminates the sequence of digits
		 */
		unsigned char _read_number(unsigned &result)
		{
			result = 0;

			for (;;) {
				unsigned char c = _poll_char();

				if (!is_digit(c))
					return c;

				result = result*10 + digit(c);
			}
		}

		/**
		 * Try to detect the size of the terminal
		 */
		Size _detect_size()
		{
			/*
			 * Set cursor position to (hopefully) exceed the terminal
			 * dimensions.
			 */
			_put_string("\033[1;199r\033[199;255H");

			/* flush incoming characters */
			for (; _driver.char_avail(); _driver.get_char());

			/* request cursor coordinates */
			_put_string("\033[6n");

			unsigned width = 0, height = 0;

			if (_poll_char()         == 27
			 && _poll_char()         == '['
			 && _read_number(height) == ';'
			 && _read_number(width)  == 'R') {

				log("detected terminal size ", width, "x", height);
				return Size(width, height);
			}

			return Size(0, 0);
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Env &env, Uart::Driver_factory &driver_factory,
		                  unsigned index, unsigned baudrate, bool detect_size)
		:
			_io_buffer(env.ram(), env.rm(), IO_BUFFER_SIZE),
			_driver_factory(driver_factory),
			_driver(_driver_factory.create(index, baudrate, _char_avail)),
			_size(detect_size ? _detect_size() : Size(0, 0))
		{ }


		/****************************
		 ** Uart session interface **
		 ****************************/

		void baud_rate(size_t bits_per_second) override
		{
			_driver.baud_rate(bits_per_second);
		}


		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() override { return _size; }

		bool avail() override { return _driver.char_avail(); }

		Genode::size_t _read(Genode::size_t dst_len)
		{
			char *io_buf      = _io_buffer.local_addr<char>();
			Genode::size_t sz = Genode::min(dst_len, _io_buffer.size());

			Genode::size_t n = 0;
			while ((n < sz) && _driver.char_avail())
				io_buf[n++] = _driver.get_char();

			return n;
		}

		Genode::size_t _write(Genode::size_t num_bytes)
		{
			/* constrain argument to I/O buffer size */
			num_bytes = Genode::min(num_bytes, _io_buffer.size());

			char const *io_buf = _io_buffer.local_addr<char>();
			for (Genode::size_t i = 0; i < num_bytes; i++)
				_driver.put_char(io_buf[i]);

			return num_bytes;
		}

		Genode::Dataspace_capability _dataspace() {
			return _io_buffer.cap(); }

		void connected_sigh(Genode::Signal_context_capability sigh) override
		{
			/*
			 * Immediately reflect connection-established signal to the
			 * client because the session is ready to use immediately after
			 * creation.
			 */
			Genode::Signal_transmitter(sigh).submit();
		}

		void read_avail_sigh(Genode::Signal_context_capability sigh) override
		{
			_char_avail.sigh = sigh;

			if (_driver.char_avail()) _char_avail();
		}

		void size_changed_sigh(Genode::Signal_context_capability) override { }

		Genode::size_t read(void *, Genode::size_t) override { return 0; }
		Genode::size_t write(void const *, Genode::size_t) override { return 0; }
};


class Uart::Root : public Uart::Root_component
{
	private:

		Env                   &_env;
		Driver_factory        &_driver_factory;
		Attached_rom_dataspace _config { _env, "config" };

	protected:

		Session_component *_create_session(const char *args) override
		{
			Session_label  const label = label_from_args(args);
			Session_policy const policy(label, _config.xml());

			unsigned const index       = policy.attribute_value("uart",        0U);
			unsigned const baudrate    = policy.attribute_value("baudrate",    0U);
			bool     const detect_size = policy.attribute_value("detect_size", false);

			return new (md_alloc())
				Session_component(_env, _driver_factory, index,
				                  baudrate, detect_size);
		}

	public:

		Root(Env &env, Allocator &md_alloc, Driver_factory &driver_factory)
		: Root_component(env.ep(), md_alloc), _env(env),
		  _driver_factory(driver_factory) { }
};

#endif /* _UART_COMPONENT_H_ */
