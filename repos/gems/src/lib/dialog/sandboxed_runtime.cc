/*
 * \brief  Runtime for hosting GUI dialogs in child components
 * \author Norman Feske
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <dialog/sandboxed_runtime.h>
#include <base/attached_ram_dataspace.h>
#include <gui_session/connection.h>
#include <input/component.h>

using namespace Dialog;


static bool click(Input::Event const &event)
{
	bool result = false;

	if (event.key_press(Input::BTN_LEFT))
		result = true;

	event.handle_touch([&] (Input::Touch_id id, float, float) {
		if (id.value == 0)
			result = true; });

	return result;
}


static bool clack(Input::Event const &event)
{
	bool result = false;

	if (event.key_release(Input::BTN_LEFT))
		result = true;

	event.handle_touch_release([&] (Input::Touch_id id) {
		if (id.value == 0)
			result = true; });

	return result;
}


struct Sandboxed_runtime::Gui_session : Session_object<Gui::Session>
{
	Env &_env;

	View &_view;

	Registry<Gui_session>::Element _element;

	using View_capability = Gui::View_capability;

	Gui::Connection _connection;

	Input::Session_component _input_component { _env, _env.ram() };

	Signal_handler<Gui_session> _input_handler {
		_env.ep(), *this, &Gui_session::_handle_input };

	bool _clicked = false;

	void _handle_input()
	{
		_connection.input()->for_each_event([&] (Input::Event const &ev) {

			/*
			 * Assign new event sequence number, pass seq event to menu view
			 * to ensure freshness of hover information.
			 */
			bool const orig_clicked = _clicked;

			if (click(ev)) _clicked = true;
			if (clack(ev)) _clicked = false;

			if (orig_clicked != _clicked) {
				Event::Seq_number &global_seq = _view._runtime._global_seq_number;
				global_seq.value++;
				_input_component.submit(Input::Seq_number { global_seq.value });
			}

			/* local event (click/clack) handling */
			_view._handle_input_event(ev);

			/* forward event to menu_view */
			_input_component.submit(ev);
		});
	}

	template <typename... ARGS>
	Gui_session(Env &env, View &view, ARGS &&... args)
	:
		Session_object(args...),
		_env(env), _view(view),
		_element(_view._gui_sessions, *this),
		_connection(env, _label.string())
	{
		_connection.input()->sigh(_input_handler);
		_env.ep().manage(_input_component);
		_input_component.event_queue().enabled(true);
	}

	~Gui_session() { _env.ep().dissolve(_input_component); }

	void upgrade(Session::Resources const &resources)
	{
		_connection.upgrade(resources);
	}

	Framebuffer::Session_capability framebuffer_session() override {
		return _connection.framebuffer_session(); }

	Input::Session_capability input_session() override {
		return _input_component.cap(); }

	View_handle create_view() override {
		return _connection.create_view(); }

	View_handle create_child_view(View_handle parent) override {
		return _connection.create_child_view(parent); }

	void destroy_view(View_handle view) override {
		_connection.destroy_view(view); }

	View_handle view_handle(View_capability view_cap, View_handle handle) override {
		return _connection.view_handle(view_cap, handle); }

	View_capability view_capability(View_handle view) override {
		return _connection.view_capability(view); }

	void release_view_handle(View_handle view) override {
		_connection.release_view_handle(view); }

	Dataspace_capability command_dataspace() override {
		return _connection.command_dataspace(); }

	void execute() override {
		_connection.execute(); }

	Framebuffer::Mode mode() override {
		return _connection.mode(); }

	void mode_sigh(Signal_context_capability sigh) override {
		_connection.mode_sigh(sigh); }

	void buffer(Framebuffer::Mode mode, bool use_alpha) override
	{
		/*
		 * Do not call 'Connection::buffer' to avoid paying session quota
		 * from our own budget.
		 */
		_connection.Session_client::buffer(mode, use_alpha);
	}

	void focus(Capability<Gui::Session> session) override {
		_connection.focus(session); }
};


Sandboxed_runtime::Sandboxed_runtime(Env &env, Allocator &alloc, Sandbox &sandbox,
                                     Attr const &attr)
:
	_env(env), _alloc(alloc), _sandbox(sandbox),
	_gui_service   (_sandbox, _gui_handler),
	_rom_service   (_sandbox, _rom_handler),
	_report_service(_sandbox, _report_handler),
	_menu_view_state { .name = attr.name }
{ }


bool Sandboxed_runtime::apply_sandbox_state(Xml_node const &state)
{
	bool reconfiguration_needed = false;

	state.for_each_sub_node("child", [&] (Xml_node const &child) {
		if (_menu_view_state.apply_child_state_report(child))
			reconfiguration_needed = true; });

	return reconfiguration_needed;
}


void Sandboxed_runtime::_handle_rom_service()
{
	_rom_service.for_each_requested_session([&] (Rom_service::Request &request) {
		_views.with_element(request.label.last_element(),
			[&] (View &view) {
				request.deliver_session(view._dialog_rom_session); },
			[&] { });
	});

	_rom_service.for_each_session_to_close([&] (Dynamic_rom_session &) {
		warning("closing of Dynamic_rom_session session not handled");
		return Rom_service::Close_response::CLOSED;
	});
}


void Sandboxed_runtime::_handle_report_service()
{
	_report_service.for_each_requested_session([&] (Report_service::Request &request) {
		if (request.label == Start_name { _menu_view_state.name, " -> hover" }) {
			_hover_report_session.construct(_env, _hover_handler, _env.ep(),
			                                request.resources, "", request.diag);
			request.deliver_session(*_hover_report_session);
		}
	});

	_report_service.for_each_session_to_close([&] (Report_session &) {
		warning("closing of Report_session not handled");
		return Report_service::Close_response::CLOSED;
	});
}


void Sandboxed_runtime::_handle_gui_service()
{
	_gui_service.for_each_requested_session([&] (Gui_service::Request &request) {
		_views.with_element(request.label.last_element(),
			[&] (View &view) {
				Gui_session &session = *new (_alloc)
					Gui_session(_env, view, _env.ep(),
					            request.resources, "", request.diag);
					request.deliver_session(session);
				},
			[&] {
				warning("unexpected GUI-sesssion request, label=", request.label);
			});
	});

	_gui_service.for_each_upgraded_session([&] (Gui_session &session,
	                                            Session::Resources const &amount) {
		session.upgrade(amount);
		return Gui_service::Upgrade_response::CONFIRMED;
	});

	_gui_service.for_each_session_to_close([&] (Gui_session &session) {
		destroy(_alloc, &session);
		return Gui_service::Close_response::CLOSED;
	});
}


void Sandboxed_runtime::gen_start_nodes(Xml_generator &xml) const
{
	_menu_view_state.gen_start_node(xml, _views);
}


void Sandboxed_runtime::Menu_view_state::gen_start_node(Xml_generator &xml, Views const &views) const
{
	xml.node("start", [&] () {

		xml.attribute("name",    name);
		xml.attribute("version", version);
		xml.attribute("caps",    caps.value);

		xml.node("resource", [&] () {
			xml.attribute("name", "RAM");
			Number_of_bytes const bytes(ram.value);
			xml.attribute("quantum", String<64>(bytes)); });

		xml.node("binary", [&] () {
			xml.attribute("name", "menu_view"); });

		xml.node("config", [&] () {

			xml.node("report", [&] () {
				xml.attribute("hover", "yes"); });

			xml.node("libc", [&] () {
				xml.attribute("stderr", "/dev/log"); });

			xml.node("vfs", [&] () {
				xml.node("tar", [&] () {
					xml.attribute("name", "menu_view_styles.tar"); });
				xml.node("dir", [&] () {
					xml.attribute("name", "dev");
					xml.node("log", [&] () { });
				});
				xml.node("dir", [&] () {
					xml.attribute("name", "fonts");
					xml.node("fs", [&] () {
						xml.attribute("label", "fonts");
					});
				});
			});

			views.for_each([&] (View const &view) {
				view._gen_menu_view_dialog(xml); });
		});

		xml.node("route", [&] () {

			views.for_each([&] (View const &view) {
				view._gen_menu_view_routes(xml); });

			xml.node("service", [&] () {
				xml.attribute("name", "Report");
				xml.attribute("label", "hover");
				xml.node("local", [&] () { });
			});

			xml.node("service", [&] () {
				xml.attribute("name", "Gui");
				xml.node("local", [&] () { });
			});

			xml.node("service", [&] () {
				xml.attribute("name", "File_system");
				xml.attribute("label", "fonts");
				xml.node("parent", [&] () {
					xml.attribute("label", "fonts"); });
			});

			auto parent_route = [&] (auto const &service)
			{
				xml.node("service", [&] {
					xml.attribute("name", service);
					xml.node("parent", [&] { }); });
			};

			parent_route("PD");
			parent_route("CPU");
			parent_route("LOG");
			parent_route("Timer");

			auto parent_rom_route = [&] (auto const &name)
			{
				xml.node("service", [&] () {
					xml.attribute("name", "ROM");
					xml.attribute("label_last", name);
					xml.node("parent", [&] { }); });
			};

			parent_rom_route("menu_view");
			parent_rom_route("ld.lib.so");
			parent_rom_route("libc.lib.so");
			parent_rom_route("libm.lib.so");
			parent_rom_route("libpng.lib.so");
			parent_rom_route("zlib.lib.so");
			parent_rom_route("vfs.lib.so");
			parent_rom_route("menu_view_styles.tar");
		});
	});
}


void Sandboxed_runtime::View::_gen_menu_view_dialog(Xml_generator &xml) const
{
	xml.node("dialog", [&] {
		xml.attribute("name", name);

		if (xpos)       xml.attribute("xpos",   xpos);
		if (ypos)       xml.attribute("ypos",   ypos);
		if (min_width)  xml.attribute("width",  min_width);
		if (min_height) xml.attribute("height", min_height);
		if (opaque)     xml.attribute("opaque", "yes");

		xml.attribute("background", String<20>(background));
	});
}


void Sandboxed_runtime::View::_gen_menu_view_routes(Xml_generator &xml) const
{
	xml.node("service", [&] {
		xml.attribute("name", "ROM");
		xml.attribute("label", name);
		xml.node("local", [&] { });
	});
}


void Sandboxed_runtime::View::_handle_input_event(Input::Event const &event)
{
	if (event.absolute_motion()) _hover_observable_without_click = true;
	if (event.touch())           _hover_observable_without_click = false;

	Event::Seq_number const global_seq = _runtime._global_seq_number;

	if (click(event) && !_click_seq_number.constructed()) {
		_click_seq_number.construct(global_seq);
		_click_delivered = false;
	}

	if (clack(event))
		_clack_seq_number.construct(global_seq);

	_try_handle_click_and_clack();

	_runtime._optional_event_handler.handle_event(Event { global_seq, event });
}


void Sandboxed_runtime::_handle_hover()
{
	if (!_hover_report_session.constructed())
		return;

	using Name = Top_level_dialog::Name;
	Name const orig_hovered_dialog = _hovered_dialog;

	_hover_report_session->with_xml([&] (Xml_node const &hover) {
		_hover_seq_number = { hover.attribute_value("seq_number", 0U) };

		hover.with_sub_node("dialog",
			[&] (Xml_node const &dialog) {
				_hovered_dialog = dialog.attribute_value("name", Name()); },
			[&] { _hovered_dialog = { }; });
	});

	if (orig_hovered_dialog.valid() && orig_hovered_dialog != _hovered_dialog)
		_views.with_element(orig_hovered_dialog,
			[&] (View &view) { view._leave(); },
			[&] { });

	if (_hovered_dialog.valid())
		_views.with_element(_hovered_dialog,
			[&] (View &view) { view._handle_hover(); },
			[&] { });
}


void Sandboxed_runtime::View::_handle_hover()
{
	_dialog_hovered = true;

	if (_click_delivered && _click_seq_number.constructed()) {
		_with_dialog_hover([&] (Xml_node const &hover) {
			Dragged_at at(*_click_seq_number, hover);
			_dialog.drag(at);
		});
	}

	_dialog_rom_session.trigger_update();
	_try_handle_click_and_clack();
}


void Sandboxed_runtime::View::_leave()
{
	_dialog_hovered = false;
	_dialog_rom_session.trigger_update();
}


void Sandboxed_runtime::View::_try_handle_click_and_clack()
{
	Constructible<Event::Seq_number> &click = _click_seq_number,
	                                 &clack = _clack_seq_number;

	if (!_click_delivered && click.constructed() && *click == _runtime._hover_seq_number) {
		_with_dialog_hover([&] (Xml_node const &hover) {
			Clicked_at at(*click, hover);
			_dialog.click(at);
			_click_delivered = true;
		});
	}

	if (click.constructed() && clack.constructed() && *clack == _runtime._hover_seq_number) {
		_with_dialog_hover([&] (Xml_node const &hover) {
			/* use click seq number for to associate clack with click */
			Clacked_at at(*click, hover);
			_dialog.clack(at);
		});

		click.destruct();
		clack.destruct();
	}
}


Sandboxed_runtime::View::~View()
{
	_gui_sessions.for_each([&] (Gui_session &session) {
		destroy(_runtime._alloc, &session); });
}
