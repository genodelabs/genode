/*
 * \brief  Mbox for communicating between Videocore and ARM
 * \author Norman Feske
 * \date   2013-09-14
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__PLATFORM__SPEC__RPI__MBOX_H_
#define _DRIVERS__PLATFORM__SPEC__RPI__MBOX_H_

/* Genode includes */
#include <util/mmio.h>
#include <base/env.h>
#include <os/attached_mmio.h>
#include <base/attached_ram_dataspace.h>
#include <base/log.h>
#include <dataspace/client.h>
#include <timer_session/connection.h>

class Mbox : Genode::Attached_mmio
{
	private:

		Genode::Env &_env;

		enum { verbose = false };

		typedef Genode::addr_t           addr_t;
		typedef Genode::uint32_t         uint32_t;
		typedef Genode::Dataspace_client Dataspace_client;

		enum { BASE = 0x2000b800,
		       SIZE = 0x100 };

		struct Read : Register<0x80, 32> { };

		struct Status : Register<0x98, 32>
		{
			struct Rd_empty : Bitfield<30, 1> { };
			struct Wr_full  : Bitfield<31, 1> { };
		};

		struct Write : Register<0xa0, 32>
		{
			struct Channel      : Bitfield<0, 4>  { };
			struct Value        : Bitfield<4, 26> { };
			struct Cache_policy : Bitfield<30, 2> { };
		};

		enum { MSG_BUFFER_SIZE = 0x1000 };
		Genode::Attached_ram_dataspace _msg_buffer { _env.ram(), _env.rm(),
		                                             MSG_BUFFER_SIZE };

		addr_t const _msg_phys = { Dataspace_client(_msg_buffer.cap()).phys_addr() };

		struct Delayer : Mmio::Delayer
		{
			Timer::Connection timer;
			void usleep(unsigned us) { timer.usleep(us); }

			Delayer(Genode::Env &env) : timer(env) { }
		} _delayer { _env };

		template <typename MESSAGE>
		MESSAGE &_message()
		{
			return *_msg_buffer.local_addr<MESSAGE>();
		}

	public:

		Mbox(Genode::Env &env)
		: Genode::Attached_mmio(env, BASE, SIZE), _env(env) { }

		/**
		 * Return reference to typed message buffer
		 */
		template <typename MESSAGE, typename... ARGS>
		MESSAGE &message(ARGS... args)
		{
			return *(new (_msg_buffer.local_addr<void>()) MESSAGE(args...));
		}

		template <typename MESSAGE>
		void call()
		{
			_message<MESSAGE>().finalize();

			if (verbose)
				_message<MESSAGE>().dump("Input");

			/* flush pending data in the read buffer */
			while (!read<Status::Rd_empty>())
				read<Read>();

			if (!wait_for<Status::Wr_full>(0, _delayer, 500, 1)) {
				Genode::error("Mbox: timeout waiting for ready-to-write");
				return;
			}

			Write::access_t value = 0;
			Write::Channel::     set(value, MESSAGE::channel());
			Write::Value::       set(value, _msg_phys >> Write::Value::SHIFT);
			Write::Cache_policy::set(value, MESSAGE::cache_policy());
			write<Write>(value);

			if (!wait_for<Status::Rd_empty>(0, _delayer, 500, 1)) {
				Genode::error("Mbox: timeout waiting for response");
				return;
			}

			if (verbose)
				_message<MESSAGE>().dump("Output");
		}
};

#endif /* _DRIVERS__PLATFORM__SPEC__RPI__MBOX_H_ */
