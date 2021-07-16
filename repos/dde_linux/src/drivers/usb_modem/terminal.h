/*
 * \brief  Service providing the 'Terminal_session' interface for a Linux file
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2020-07-09
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__USB_MODEM__TERMINAL_H_
#define _SRC__DRIVERS__USB_MODEM__TERMINAL_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/log.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>

#include <legacy/lx_kit/scheduler.h>

namespace Terminal {
	class Session_component;
	class Root;
}

extern "C" {
	struct usb_class_driver;
}

class Terminal::Session_component : public Genode::Rpc_object<Session, Session_component>
{
	using size_t = Genode::size_t;

	private:

		enum State { WRITE, READ };

		Genode::Attached_ram_dataspace    _io_buffer;
		Genode::Signal_context_capability _read_avail_sigh { };

		State  _state { WRITE };
		size_t _data_avail { 0 };
		size_t _io_buffer_size;
		void   *_wdm_device { nullptr };
		usb_class_driver *_class_driver { nullptr };

		static void _run_wdm_device(void *args);
		static void _run_wdm_write(void *args);
		static void _run_wdm_read(void *args);

		Lx::Task _task_write { _run_wdm_write, this, "wdm_task_write",
		                       Lx::Task::PRIORITY_1, Lx::scheduler() };

		Lx::Task _task_read { _run_wdm_read, this, "wdm_task_read",
		                      Lx::Task::PRIORITY_1, Lx::scheduler() };

		Lx::Task _task_device { _run_wdm_device, this, "wdm_task_devie",
		                        Lx::Task::PRIORITY_1, Lx::scheduler() };
		void _schedule_read()
		{
			_task_read.unblock();
		}

	public:

		Session_component(Genode::Env &env,
		                  Genode::size_t io_buffer_size,
		                  usb_class_driver *class_driver);

		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() override { return Size(0, 0); }

		bool avail() override
		{
			return _data_avail > 0;
		}

		Genode::size_t _read(Genode::size_t dst_len)
		{
			if (_state != READ) return 0;

			size_t length = Genode::min(dst_len, _data_avail);
			if (dst_len < _data_avail)
				Genode::warning("dst_len < data_avail (", dst_len, " < ", _data_avail, ") not supported");

			_data_avail -= length;

			if (_data_avail == 0) {
				_state = WRITE;
				_schedule_read();
			}

			return length;
		}

		Genode::size_t _write(Genode::size_t num_bytes)
		{
			if (_state == READ) return 0;

			_data_avail = num_bytes;
			_state = WRITE;

			_task_write.unblock();
			Lx::scheduler().schedule();

			return 0;
		}

		Genode::Dataspace_capability _dataspace()
		{
			return _io_buffer.cap();
		}

		void read_avail_sigh(Genode::Signal_context_capability sigh) override
		{
			_read_avail_sigh = sigh;
		}

		void connected_sigh(Genode::Signal_context_capability sigh) override
		{
			Genode::Signal_transmitter(sigh).submit();
	}

		void size_changed_sigh(Genode::Signal_context_capability) override { }

		size_t read(void *, size_t) override { return 0; }
		size_t write(void const *, size_t) override { return 0; }

		char *buffer() { return _io_buffer.local_addr<char>(); }

		void signal_data_avail()
		{
			if (_read_avail_sigh.valid() == false) return;
			_state = READ;
			Genode::Signal_transmitter(_read_avail_sigh).submit();
		}
};


class Terminal::Root : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env      &_env;
		usb_class_driver *_class_driver { nullptr };

	protected:

		Session_component *_create_session(const char *args) override
		{
			Genode::size_t const io_buffer_size = 4096ul;
			return new (md_alloc())
			       Session_component(_env, io_buffer_size, _class_driver);
		}

	public:

		/**
		 * Constructor
		 */
		Root(Genode::Env       &env,
		     Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }

		void class_driver(usb_class_driver *class_driver) { _class_driver = class_driver; }
};

#endif /* _SRC__DRIVERS__USB_MODEM__TERMINAL_H_ */
