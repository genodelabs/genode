/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \author Pirmin Duss
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SSH_TERMINAL_SESSION_COMPONENT_H_
#define _SSH_TERMINAL_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/capability.h>
#include <base/signal.h>

/* local includes */
#include "util.h"
#include "terminal.h"


namespace Terminal {
	class Session_component;
};

class Terminal::Session_component : public Genode::Rpc_object<Session, Session_component>,
                                    public Ssh::Terminal
{
	private:

		Genode::Attached_ram_dataspace _io_buffer;

	public:

		Session_component(Genode::Env &env,
		                  Genode::size_t io_buffer_size,
		                  Ssh::User const &user)
		:
			Ssh::Terminal(user),
			_io_buffer(env.ram(), env.rm(), io_buffer_size)
		{ }

		virtual ~Session_component() = default;

		/********************************
		 ** Terminal session interface **
		 ********************************/

		Genode::size_t read(void *buf, Genode::size_t)        override { return 0; }
		Genode::size_t write(void const *buf, Genode::size_t) override { return 0; }

		Size size()  override { return Ssh::Terminal::size(); }
		bool avail() override { return !Ssh::Terminal::read_buffer_empty(); }

		void read_avail_sigh(Genode::Signal_context_capability sigh) override {
			Ssh::Terminal::read_avail_sigh(sigh);
		}

		void connected_sigh(Genode::Signal_context_capability sigh) override {
			Ssh::Terminal::connected_sigh(sigh);
		}

		void size_changed_sigh(Genode::Signal_context_capability sigh) override {
			Ssh::Terminal::size_changed_sigh(sigh);
		}

		Genode::Dataspace_capability _dataspace() { return _io_buffer.cap(); }

		Genode::size_t _read(Genode::size_t num)
		{
			Genode::size_t num_bytes = 0;
			Libc::with_libc([&] () {
				char *buf = _io_buffer.local_addr<char>();
				num = Genode::min(_io_buffer.size(), num);
				num_bytes = Ssh::Terminal::read(buf, num);
			});
			return num_bytes;
		}

		Genode::size_t _write(Genode::size_t num)
		{
			ssize_t written = 0;
			Libc::with_libc([&] () {
				char *buf = _io_buffer.local_addr<char>();
				num = Genode::min(num, _io_buffer.size());
				written = Ssh::Terminal::write(buf, num);

				if (written < 0) {
					Genode::error("write error, dropping data");
					written = 0;
				}
			});
			return written;
		}
};

#endif  /* _SSH_TERMINAL_SESSION_COMPONENT_H_ */
