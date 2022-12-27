/*
 * \brief  GUI wrapper for monitoring the user input of GUI components
 * \author Norman Feske
 * \date   2020-01-12
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GUI_H_
#define _GUI_H_

/* Genode includes */
#include <input/component.h>
#include <base/session_object.h>
#include <gui_session/connection.h>

/* local includes */
#include <input_event_handler.h>

namespace Gui {

	using namespace Genode;

	struct Session_component;
}


struct Gui::Session_component : Session_object<Gui::Session>
{
	Env &_env;

	Input_event_handler &_event_handler;

	Input::Seq_number &_global_seq_number;

	Gui::Connection _connection;

	Input::Session_component _input_component { _env, _env.ram() };

	Signal_handler<Session_component> _input_handler {
		_env.ep(), *this, &Session_component::_handle_input };

	void _handle_input()
	{
		_connection.input()->for_each_event([&] (Input::Event ev) {

			/*
			 * Augment input stream with sequence numbers to correlate
			 * clicks with hover reports.
			 */
			if (ev.touch() || ev.touch_release()) {
				_global_seq_number.value++;
				_input_component.submit(_global_seq_number);
			}

			/*
			 * Feed touch coordinates of primary finger as absolute motion to
			 * the menu view to trigger an update of the hover report.
			 */
			ev.handle_touch([&] (Input::Touch_id id, float x, float y) {
				Input::Absolute_motion const xy { (int)x, (int)y };
				if (id.value == 0)
					_input_component.submit(xy); });

			_event_handler.handle_input_event(ev);
		});
	}

	template <typename... ARGS>
	Session_component(Env &env, Input_event_handler &event_handler,
	                  Input::Seq_number &global_seq_number, ARGS &&... args)
	:
		Session_object(args...),
		_env(env), _event_handler(event_handler),
		_global_seq_number(global_seq_number),
		_connection(env, _label.string())
	{
		_connection.input()->sigh(_input_handler);
		_env.ep().manage(_input_component);
		_input_component.event_queue().enabled(true);
	}

	~Session_component() { _env.ep().dissolve(_input_component); }

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

#endif /* _GUI_H_ */
