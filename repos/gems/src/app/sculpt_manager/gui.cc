/*
 * \brief  GUI wrapper for monitoring the user input of GUI components
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "gui.h"

/* Genode includes */
#include <input/component.h>
#include <gui_session/connection.h>
#include <base/heap.h>


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


struct Gui::Session_component : Rpc_object<Gui::Session>
{
	Env &_env;

	Input_event_handler &_event_handler;

	Input::Seq_number &_global_input_seq_number;

	Genode::Connection<Gui::Session> _connection;

	Gui::Session_client _gui_session { _connection.cap() };

	Input::Session_client _gui_input { _env.rm(), _gui_session.input() };

	Input::Session_component _input_component { _env, _env.ram() };

	Signal_handler<Session_component> _input_handler {
		_env.ep(), *this, &Session_component::_handle_input };

	bool _clicked = false;

	void _handle_input()
	{
		_gui_input.for_each_event([&] (Input::Event ev) {

			/*
			 * Assign new event sequence number, pass seq event to menu view
			 * to ensure freshness of hover information.
			 */
			bool const orig_clicked = _clicked;

			if (click(ev)) _clicked = true;
			if (clack(ev)) _clicked = false;

			if (orig_clicked != _clicked) {
				_global_input_seq_number.value++;
				_input_component.submit(_global_input_seq_number);
			}

			/* handle event locally within the sculpt manager */
			_event_handler.handle_input_event(ev);

			_input_component.submit(ev);
		});
	}

	Session_component(Env &env, char const *args,
	                  Input_event_handler &event_handler,
	                  Input::Seq_number   &global_input_seq_number)
	:
		_env(env), _event_handler(event_handler),
		_global_input_seq_number(global_input_seq_number),
		_connection(env, session_label_from_args(args), Ram_quota { 36*1024 }, { })
	{
		_gui_input.sigh(_input_handler);
		_env.ep().manage(_input_component);
		_input_component.event_queue().enabled(true);
	}

	~Session_component() { _env.ep().dissolve(_input_component); }

	void upgrade(Session::Resources const &resources)
	{
		_connection.upgrade(resources);
	}

	Framebuffer::Session_capability framebuffer() override {
		return _gui_session.framebuffer(); }

	Input::Session_capability input() override {
		return _input_component.cap(); }

	View_result view(View_id id, View_attr const &attr) override {
		return _gui_session.view(id, attr); }

	Child_view_result child_view(View_id id, View_id parent, View_attr const &attr) override {
		return _gui_session.child_view(id, parent, attr); }

	void destroy_view(View_id view) override {
		_gui_session.destroy_view(view); }

	Associate_result associate(View_id id, View_capability view_cap) override {
		return _gui_session.associate(id, view_cap); }

	View_capability view_capability(View_id view) override {
		return _gui_session.view_capability(view); }

	void release_view_id(View_id view) override {
		_gui_session.release_view_id(view); }

	Dataspace_capability command_dataspace() override {
		return _gui_session.command_dataspace(); }

	void execute() override {
		_gui_session.execute(); }

	Framebuffer::Mode mode() override {
		return _gui_session.mode(); }

	void mode_sigh(Signal_context_capability sigh) override {
		_gui_session.mode_sigh(sigh); }

	Buffer_result buffer(Framebuffer::Mode mode, bool use_alpha) override {
		return _gui_session.buffer(mode, use_alpha); }

	void focus(Capability<Gui::Session> session) override {
		_gui_session.focus(session); }
};


Gui::Session_component *Gui::Root::_create_session(const char *args)
{
	return new (md_alloc()) Session_component(_env, args, _event_handler,
	                                          _global_input_seq_number);
}


void Gui::Root::_upgrade_session(Session_component *session, const char *args)
{
	session->upgrade(session_resources_from_args(args));
}


void Gui::Root::_destroy_session(Session_component *session)
{
	Genode::destroy(md_alloc(), session);
}


Gui::Root::Root(Env &env, Allocator &md_alloc,
                Input_event_handler &event_handler,
                Input::Seq_number   &global_input_seq_number)
:
	Root_component<Session_component>(env.ep(), md_alloc),
	_env(env), _event_handler(event_handler),
	_global_input_seq_number(global_input_seq_number)
{
	env.parent().announce(env.ep().manage(*this));
}


Gui::Root::~Root() { _env.ep().dissolve(*this); }

