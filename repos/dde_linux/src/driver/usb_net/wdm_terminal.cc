/*
 * \brief  Service providing the 'Terminal_session' interface for cdc-wdm file
 * \author Sebastian Sumpf
 * \date   2023-08-16
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_ram_dataspace.h>
#include <base/log.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>

#include <lx_emul/task.h>
#include <lx_kit/env.h>
#include <usb_net.h>

namespace Terminal {
	class Session_component;
	class Root;
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

		struct lx_wdm _wdm_data { &_data_avail, buffer(), this, 1 };

		task_struct *_write_task {
			lx_user_new_usb_task(lx_wdm_write, &_wdm_data, "wdm_write")};

		task_struct *_read_task {
			lx_user_new_usb_task(lx_wdm_read, &_wdm_data,  "wdm_read")};

		task_struct *_device_task {
			lx_user_new_usb_task(lx_wdm_device, nullptr, "device_task")};

		/* non-copyable */
		Session_component(const Session_component&) = delete;
		Session_component & operator=(const Session_component&) = delete;

	public:

		Session_component(Genode::Env &env,
		                  Genode::size_t io_buffer_size)
		:
			_io_buffer(env.ram(), env.rm(), io_buffer_size)
		{ }

		void schedule_read()
		{
			lx_emul_task_unblock(_read_task);
		}

		void schedule_write()
		{
			lx_emul_task_unblock(_write_task);
		}

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
				schedule_read();
			}

			return length;
		}

		Genode::size_t _write(Genode::size_t num_bytes)
		{
			if (_state == READ) return 0;

			_data_avail = num_bytes;
			_state = WRITE;

			schedule_write();
			Lx_kit::env().scheduler.execute();

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

		void *buffer() { return _io_buffer.local_addr<void>(); }

		void signal_data_avail()
		{
			if (_read_avail_sigh.valid() == false) return;
			_state = READ;
			Genode::Signal_transmitter(_read_avail_sigh).submit();
		}
};


class Terminal::Root : public Genode::Root_component<Session_component, Genode::Single_client>
{
	private:

		Genode::Env &_env;

		task_struct *_create_task  {
			lx_user_new_usb_task(_create_session, this, "terminal_session")};

		Genode::Constructible<Session_component> _session { };

		static int _create_session(void *arg)
		{
			Root *root = static_cast<Root *>(arg);
			Genode::size_t const io_buffer_size = 4096ul;

			while (true) {
				lx_emul_task_schedule(true);
				if (!root->_session.constructed())
					root->_session.construct(root->_env, io_buffer_size);
			}
		}

		/* non-copyable */
		Root(const Root&) = delete;
		Root & operator=(const Root&) = delete;

	protected:

		Session_component *_create_session(const char *) override
		{
			if (!_session.constructed()) {
				lx_emul_task_unblock(_create_task);
				Lx_kit::env().scheduler.execute();
			}
			return &*_session;
		}

	public:

		Root(Genode::Env       &env,
		     Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component, Genode::Single_client>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{
			_env.parent().announce(_env.ep().manage(*this));
		}
};


void lx_wdm_schedule_read(void *handle)
{
	using namespace Terminal;
	Session_component *session = static_cast<Session_component *>(handle);
	session->schedule_read();
}


void lx_wdm_signal_data_avail(void *handle)
{
	using namespace Terminal;
	Session_component *session = static_cast<Session_component *>(handle);
	session->signal_data_avail();
}


void lx_wdm_create_root(void)
{
	new (Lx_kit::env().heap) Terminal::Root(Lx_kit::env().env, Lx_kit::env().heap);
}
