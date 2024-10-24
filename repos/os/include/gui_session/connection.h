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
#include <rom_session/client.h>
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

		/*
		 * Session quota at the construction time of the connection
		 *
		 * The 'Gui::Session::CAP_QUOTA' value is based the needs of the
		 * nitpicker GUI server. To accommodate the common case where a client
		 * is served by the wm, which in turn wraps a nitpicker session, extend
		 * the session quota according to the needs of the wm.
		 */
		static constexpr Ram_quota _RAM_QUOTA { 96*1024 };
		static constexpr Cap_quota _CAP_QUOTA { Session::CAP_QUOTA + 9 };

		Constructible<Rom_session_client> _info_rom { };
		Constructible<Attached_dataspace> _info_ds  { };

		Rom_session_capability _info_rom_capability()
		{
			for (;;) {
				using Error = Session::Info_error;
				auto const result = _client.info();
				if (result == Error::OUT_OF_RAM)  { upgrade_ram(8*1024); continue; }
				if (result == Error::OUT_OF_CAPS) { upgrade_caps(2);     continue; }
				return result.convert<Rom_session_capability>(
					[&] (Rom_session_capability cap) { return cap; },
					[&] (auto) /* handled above */   { return Rom_session_capability(); });
			}
		}

		void _with_info_rom(auto const &fn)
		{
			if (!_info_ds.constructed())
				_info_rom.construct(_info_rom_capability());
			fn(*_info_rom);
		}

		void _with_info_xml(auto const &fn)
		{
			_with_info_rom([&] (Rom_session_client &rom) {
				if (!_info_ds.constructed() || rom.update() == false)
					_info_ds.construct(_env.rm(), rom.dataspace());

				try {
					Xml_node xml(_info_ds->local_addr<char>(), _info_ds->size());
					fn(xml); }
				catch (Xml_node::Invalid_syntax) {
					warning("Gui::info has invalid XML syntax"); }
			});
		}

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
			Genode::Connection<Session>(env, label, _RAM_QUOTA, _CAP_QUOTA,
			                            Affinity { }, Args { }),
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

		void buffer(Framebuffer::Mode mode)
		{
			size_t const needed  = Session_client::ram_quota(mode);
			size_t const upgrade = needed > _ram_quota.value
			                     ? needed - _ram_quota.value : 0u;
			if (upgrade > 0) {
				this->upgrade_ram(upgrade);
				_ram_quota.value += upgrade;
			}

			for (;;) {
				using Result = Session_client::Buffer_result;
				auto const result = _client.buffer(mode);
				if (result == Result::OUT_OF_RAM)  { upgrade_ram(8*1024); continue; }
				if (result == Result::OUT_OF_CAPS) { upgrade_caps(2);     continue; }
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
		 * Call 'fn' with mode information as 'Xml_node const &' argument
		 */
		void with_info(auto const &fn) { _with_info_xml(fn); }

		Capability<Rom_session> info_rom_cap()
		{
			Capability<Rom_session> result { };
			_with_info_rom([&] (Rom_session_client &rom) { result = rom.rpc_cap(); });
			return result;
		}

		using Panorama_result = Attempt<Gui::Rect, Gui::Undefined>;

		/**
		 * Return geometry of the total panorama
		 *
		 * The retured rectangle may be undefined for a client of the nitpicker
		 * GUI server in the absence of any capture client.
		 */
		Panorama_result panorama()
		{
			Gui::Rect result { };
			_with_info_xml([&] (Xml_node const &info) {
				result = Rect::from_xml(info); });
			return result.valid() ? Panorama_result { result }
			                      : Panorama_result { Undefined { } };
		}

		using Window_result = Attempt<Rect, Gui::Undefined>;

		/**
		 * Return suitable geometry of top-level view
		 *
		 * For nitpicker clients, the window is the bounding box of all capture
		 * clients. For window-manager clients, returned rectangle corresponds
		 * to the window size as defined by the layouter.
		 *
		 * The returned rectangle may be undefined when a client of the window
		 * manager has not defined a top-level view yet. Once a window is got
		 * closed, the returned rectangle is zero-sized.
		 */
		Window_result window()
		{
			Rect result { };
			bool closed = false;
			_with_info_xml([&] (Xml_node const &info) {
				Rect bb { };  /* bounding box of all captured rects */
				unsigned count = 0;
				info.for_each_sub_node("capture", [&] (Xml_node const &capture) {
					closed |= (capture.attribute_value("closed", false));
					bb = Rect::compound(bb, Rect::from_xml(capture));
					count++;
				});
				result = (count == 1) ? bb : Rect::from_xml(info);
			});
			if (closed)
				return Rect { };

			return result.valid() ? Window_result { result }
			                      : Window_result { Undefined { } };
		}

		/**
		 * Register signal handler to be notified about mode changes
		 */
		void info_sigh(Signal_context_capability sigh)
		{
			_with_info_rom([&] (Rom_session_client &rom) { rom.sigh(sigh); });
			if (sigh.valid())
				Signal_transmitter(sigh).submit();
		}

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
