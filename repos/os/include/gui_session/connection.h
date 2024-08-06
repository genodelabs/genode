/*
 * \brief  Connection to GUI service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GUI_SESSION__CONNECTION_H_
#define _INCLUDE__GUI_SESSION__CONNECTION_H_

#include <gui_session/client.h>
#include <framebuffer_session/client.h>
#include <input_session/client.h>
#include <base/connection.h>

namespace Gui { class Connection; }


class Gui::Connection : private Genode::Connection<Session>
{
	private:

		Env &_env;

		Session_client _client { cap() };

		Attached_dataspace _command_ds { _env.rm(), _client.command_dataspace() };

		using Command_buffer = Gui::Session::Command_buffer;

		Command_buffer &_command_buffer { *_command_ds.local_addr<Command_buffer>() };

		Ram_quota _ram_quota { }; /* session quota donated for virtual frame buffer */

	public:

		/**
		 * Framebuffer access
		 */
		Framebuffer::Session_client framebuffer { _client.framebuffer() };

		/**
		 * Input access
		 */
		Input::Session_client input { _env.rm(), _client.input() };

		using View_handle = Session::View_handle;
		using Genode::Connection<Session>::cap;
		using Genode::Connection<Session>::upgrade;
		using Genode::Connection<Session>::upgrade_ram;
		using Genode::Connection<Session>::upgrade_caps;

		/**
		 * Constructor
		 */
		Connection(Env &env, Session_label const &label = { })
		:
			Genode::Connection<Session>(env, label, Ram_quota { 36*1024 }, Args()),
			_env(env)
		{ }

		View_handle create_view()
		{
			View_handle result { };
			for (bool retry = false; ; ) {
				using Error = Session_client::Create_view_error;
				_client.create_view().with_result(
					[&] (View_handle handle) { result = handle; },
					[&] (Error e) {
						switch (e) {
						case Error::OUT_OF_RAM:  upgrade_ram(8*1024); retry = true; return;
						case Error::OUT_OF_CAPS: upgrade_caps(2);     retry = true; return;
						}
					});
				if (!retry)
					break;
			}
			return result;
		}

		View_handle create_child_view(View_handle parent)
		{
			View_handle result { };
			for (bool retry = false; ; ) {
				using Error = Session_client::Create_child_view_error;
				_client.create_child_view(parent).with_result(
					[&] (View_handle handle) { result = handle; },
					[&] (Error e) {
						switch (e) {
						case Error::OUT_OF_RAM:  upgrade_ram(8*1024); retry = true; return;
						case Error::OUT_OF_CAPS: upgrade_caps(2);     retry = true; return;
						case Error::INVALID:     break;
						}
						error("failed to create child view for invalid parent view");
					});
				if (!retry)
					break;
			}
			return result;
		}

		void destroy_view(View_handle view)
		{
			_client.destroy_view(view);
		}

		void release_view_handle(View_handle handle)
		{
			_client.release_view_handle(handle);
		}

		View_capability view_capability(View_handle handle)
		{
			return _client.view_capability(handle);
		}

		View_handle view_handle(View_capability view, View_handle handle = { })
		{
			View_handle result { };
			for (bool retry = false; ; ) {
				using Error = Session_client::View_handle_error;
				_client.view_handle(view, handle).with_result(
					[&] (View_handle handle) { result = handle; },
					[&] (Error e) {
						switch (e) {
						case Error::OUT_OF_RAM:  upgrade_ram(8*1024); retry = true; return;
						case Error::OUT_OF_CAPS: upgrade_caps(2);     retry = true; return;
						}
					});
				if (!retry)
					break;
			}
			return result;
		}

		void buffer(Framebuffer::Mode mode, bool use_alpha)
		{
			size_t const needed  = Session_client::ram_quota(mode, use_alpha);
			size_t const upgrade = needed > _ram_quota.value
			                     ? needed - _ram_quota.value : 0u;
			if (upgrade > 0) {
				this->upgrade_ram(upgrade);
				_ram_quota.value += upgrade;
			}

			for (bool retry = false; ; ) {
				using Result = Session_client::Buffer_result;
				auto const result = _client.buffer(mode, use_alpha);
				if (result == Result::OUT_OF_RAM)  { upgrade_ram(8*1024); retry = true; }
				if (result == Result::OUT_OF_CAPS) { upgrade_caps(2);     retry = true; }
				if (!retry)
					break;
			}
		}

		/**
		 * Enqueue command to command buffer
		 *
		 * The submitted command is not executed immediately. To execute a
		 * batch of enqueued commands, the 'execute' method must be called.
		 * Only in the corner case when there is not space left in the command
		 * buffer, the 'execute' is called to make room in the buffer.
		 */
		template <typename CMD>
		void enqueue(auto &&... args) { enqueue(Session::Command( CMD { args... } )); }

		void enqueue(Session::Command const &command)
		{
			if (_command_buffer.full())
				execute();

			_command_buffer.enqueue(command);
		}

		void execute()
		{
			_client.execute();
			_command_buffer.reset();
		}

	/**
	 * Return physical screen mode
	 */
	Framebuffer::Mode mode() { return _client.mode(); }

	/**
	 * Register signal handler to be notified about mode changes
	 */
	void mode_sigh(Signal_context_capability sigh) { _client.mode_sigh(sigh); }

	void focus(Capability<Session> focused) { _client.focus(focused); }
};

#endif /* _INCLUDE__GUI_SESSION__CONNECTION_H_ */
