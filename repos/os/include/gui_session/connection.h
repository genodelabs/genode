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

namespace Gui {
	class  Connection;
	struct Top_level_view;
}


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

		View_ids view_ids { };

		/**
		 * Framebuffer access
		 */
		Framebuffer::Session_client framebuffer { _client.framebuffer() };

		/**
		 * Input access
		 */
		Input::Session_client input { _env.rm(), _client.input() };

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

		void view(View_id id, Session::View_attr const &attr)
		{
			for (;;) {
				using Result = Session::View_result;
				switch (_client.view(id, attr)) {
				case Result::OUT_OF_RAM:  upgrade_ram(8*1024); break;
				case Result::OUT_OF_CAPS: upgrade_caps(2);     break;
				case Result::OK:
					return;
				}
			}
		}

		void child_view(View_id id, View_id parent, Session::View_attr const &attr)
		{
			for (;;) {
				using Result = Session::Child_view_result;
				switch (_client.child_view(id, parent, attr)) {
				case Result::OUT_OF_RAM:  upgrade_ram(8*1024); break;
				case Result::OUT_OF_CAPS: upgrade_caps(2);     break;
				case Result::OK:
					return;
				case Result::INVALID:
					error("failed to create child view for invalid parent view");
					return;
				}
			}
		}

		void destroy_view(View_id view)
		{
			_client.destroy_view(view);
		}

		void release_view_id(View_id id)
		{
			_client.release_view_id(id);
		}

		View_capability view_capability(View_id id)
		{
			for (;;) {
				View_capability result { };
				using Error = Session::View_capability_error;
				bool const retry = _client.view_capability(id).convert<bool>(
					[&] (View_capability cap) { result = cap; return false; },
					[&] (Error e) {
						switch (e) {
						case Error::OUT_OF_CAPS: upgrade_caps(2);     return true;
						case Error::OUT_OF_RAM:  upgrade_ram(8*1024); return true;
						}
						return false;
					});
				if (!retry)
					return result;
			}
		}

		void associate(View_id id, View_capability view)
		{
			using Result = Session::Associate_result;
			for (;;) {
				switch (_client.associate(id, view)) {
				case Result::OUT_OF_RAM:  upgrade_ram(8*1024); break;
				case Result::OUT_OF_CAPS: upgrade_caps(2);     break;
				case Result::OK:          return;
				case Result::INVALID:
					warning("attempt to create ID for invalid view");
					return;
				}
			}
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


/**
 * Helper for the common case of creating a top-level view
 */
class Gui::Top_level_view : View_ref, View_ids::Element
{
	private:

		Connection &_gui;

		Rect _rect;

	public:

		using View_ids::Element::id;

		Top_level_view(Connection &gui, Rect rect = { })
		:
			View_ids::Element(*this, gui.view_ids), _gui(gui), _rect(rect)
		{
			_gui.view(id(), { .title = { }, .rect = rect, .front = true });
		}

		~Top_level_view() { _gui.destroy_view(id()); }

		void front()
		{
			_gui.enqueue<Session::Command::Front>(id());
			_gui.execute();
		}

		void geometry(Rect rect)
		{
			_rect = rect;
			_gui.enqueue<Session::Command::Geometry>(id(), _rect);
			_gui.execute();
		}

		void area(Area area) { geometry(Rect { _rect.at, area } ); }
		void at  (Point at)  { geometry(Rect { at, _rect.area } ); }

		Area  area() const { return _rect.area; }
		Point at()   const { return _rect.at;   }
};

#endif /* _INCLUDE__GUI_SESSION__CONNECTION_H_ */
