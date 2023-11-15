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
				_view._global_seq_number.value++;
				_input_component.submit(Input::Seq_number { _view._global_seq_number.value });
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

	View_handle create_view(View_handle parent) override {
		return _connection.create_view(parent); }

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
		_connection.Client::buffer(mode, use_alpha);
	}

	void focus(Capability<Gui::Session> session) override {
		_connection.focus(session); }
};


Sandboxed_runtime::Sandboxed_runtime(Env &env, Allocator &alloc, Sandbox &sandbox)
:
	_env(env), _alloc(alloc), _sandbox(sandbox),
	_gui_service   (_sandbox, _gui_handler),
	_rom_service   (_sandbox, _rom_handler),
	_report_service(_sandbox, _report_handler)
{ }


bool Sandboxed_runtime::apply_sandbox_state(Xml_node const &state)
{
	bool reconfiguration_needed = false;

	state.for_each_sub_node("child", [&] (Xml_node const &child) {
		using Name = Top_level_dialog::Name;
		Name const name = child.attribute_value("name", Name());
		_views.with_element(name,
			[&] (View &view) {
				if (view._menu_view_state.apply_child_state_report(child))
					reconfiguration_needed = true; },
			[&] /* no view named after this child */ { });
	 });

	return reconfiguration_needed;
}


void Sandboxed_runtime::_handle_rom_service()
{
	_rom_service.for_each_requested_session([&] (Rom_service::Request &request) {
		if (request.label.last_element() == "dialog") {
			_views.with_element(request.label.prefix(),
				[&] (View &view) {
					request.deliver_session(view._dialog_rom_session); },
				[&] /* no view named after this child */ { });
		}
	});

	_rom_service.for_each_session_to_close([&] (Dynamic_rom_session &) {
		warning("closing of Dynamic_rom_session session not handled");
		return Rom_service::Close_response::CLOSED;
	});
}


void Sandboxed_runtime::_handle_report_service()
{
	_report_service.for_each_requested_session([&] (Report_service::Request &request) {
		if (request.label.last_element() == "hover") {
			_views.with_element(request.label.prefix(),
				[&] (View &view) {
					view._hover_report_session.construct(_env, view._hover_handler, _env.ep(),
					                                     request.resources, "", request.diag);
					request.deliver_session(*view._hover_report_session);
				},
				[&] /* no view named after this child */ { });
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
		_views.with_element(request.label.prefix(),
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
	_views.for_each([&] (View const &view) {
		view._menu_view_state.gen_start_node(xml); });
}


void Sandboxed_runtime::View::Menu_view_state::gen_start_node(Xml_generator &xml) const
{
	xml.node("start", [&] () {

		xml.attribute("name",    _name);
		xml.attribute("version", _version);
		xml.attribute("caps",    _caps.value);

		xml.node("resource", [&] () {
			xml.attribute("name", "RAM");
			Number_of_bytes const bytes(_ram.value);
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
		});

		xml.node("route", [&] () {

			xml.node("service", [&] () {
				xml.attribute("name", "ROM");
				xml.attribute("label", "dialog");
				xml.node("local", [&] () { });
			});

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

			xml.node("any-service", [&] () {
				xml.node("parent", [&] () { }); });
		});
	});
}


void Sandboxed_runtime::View::_handle_input_event(Input::Event const &event)
{
	if (event.absolute_motion()) _hover_observable_without_click = true;
	if (event.touch())           _hover_observable_without_click = false;

	if (click(event) && !_click_seq_number.constructed()) {
		_click_seq_number.construct(_global_seq_number);
		_click_delivered = false;
	}

	if (clack(event))
		_clack_seq_number.construct(_global_seq_number);

	_try_handle_click_and_clack();

	_optional_event_handler.handle_event(Event { _global_seq_number, event });
}


void Sandboxed_runtime::View::_handle_hover()
{
	bool const orig_dialog_hovered = _dialog_hovered;

	if (_hover_report_session.constructed())
		_hover_report_session->with_xml([&] (Xml_node const &hover) {
			_hover_seq_number = { hover.attribute_value("seq_number", 0U) };
			_dialog_hovered = (hover.num_sub_nodes() > 0);
		});

	if (orig_dialog_hovered != _dialog_hovered || _dialog_hovered)
		_dialog_rom_session.trigger_update();

	if (_click_delivered && _click_seq_number.constructed()) {
		_with_dialog_hover([&] (Xml_node const &hover) {
			Dragged_at at(*_click_seq_number, hover);
			_dialog.drag(at);
		});
	}

	_try_handle_click_and_clack();
}


void Sandboxed_runtime::View::_try_handle_click_and_clack()
{
	Constructible<Event::Seq_number> &click = _click_seq_number,
	                                 &clack = _clack_seq_number;

	if (!_click_delivered && click.constructed() && *click == _hover_seq_number) {
		_with_dialog_hover([&] (Xml_node const &hover) {
			Clicked_at at(*click, hover);
			_dialog.click(at);
			_click_delivered = true;
		});
	}

	if (click.constructed() && clack.constructed() && *clack== _hover_seq_number) {
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
		destroy(_alloc, &session); });
}
