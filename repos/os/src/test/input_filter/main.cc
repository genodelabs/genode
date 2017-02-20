/*
 * \brief  Test for input filter
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/heap.h>
#include <base/component.h>
#include <base/session_label.h>
#include <base/attached_rom_dataspace.h>
#include <input_session/connection.h>
#include <input/component.h>
#include <timer_session/connection.h>
#include <os/static_root.h>
#include <root/component.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <os/reporter.h>
#include <base/sleep.h>

namespace Test {
	class Input_from_filter;
	class Input_to_filter;
	class Input_root;
	class Main;
	using namespace Genode;
}


namespace Genode {

	static inline void print(Output &output, Input::Event const &ev)
	{
		switch (ev.type()) {
		case Input::Event::INVALID:   print(output, "INVALID");   break;
		case Input::Event::MOTION:    print(output, "MOTION");    break;
		case Input::Event::PRESS:     print(output, "PRESS");     break;
		case Input::Event::RELEASE:   print(output, "RELEASE");   break;
		case Input::Event::WHEEL:     print(output, "WHEEL");     break;
		case Input::Event::FOCUS:     print(output, "FOCUS");     break;
		case Input::Event::LEAVE:     print(output, "LEAVE");     break;
		case Input::Event::TOUCH:     print(output, "TOUCH");     break;
		case Input::Event::CHARACTER: print(output, "CHARACTER"); break;
		};

		if (ev.type() == Input::Event::PRESS || ev.type() == Input::Event::RELEASE)
			print(output, " (", Input::key_name(ev.keycode()), ")");
	}
}


class Test::Input_from_filter
{
	public:

		struct Event_handler
		{
			virtual void handle_event_from_filter(Input::Event const &) = 0;
		};

	private:

		Env &_env;

		Event_handler &_event_handler;

		Input::Connection _connection;

		bool _input_expected = false;

		void _handle_input()
		{
			if (_input_expected)
				_connection.for_each_event([&] (Input::Event const &event) {
					_event_handler.handle_event_from_filter(event); });
		}

		Signal_handler<Input_from_filter> _input_handler {
			_env.ep(), *this, &Input_from_filter::_handle_input };

	public:

		Input_from_filter(Env &env, Event_handler &event_handler)
		:
			_env(env), _event_handler(event_handler), _connection(env)
		{
			_connection.sigh(_input_handler);
		}

		void input_expected(bool expected)
		{
			_input_expected = expected;
			_handle_input();
		}
};


class Test::Input_root : public Root_component<Input::Session_component>
{
	private:

		Input::Session_component &_usb_input;
		Input::Session_component &_ps2_input;

	public:

		Input_root(Entrypoint &ep, Allocator &md_alloc,
		           Input::Session_component &usb_input,
		           Input::Session_component &ps2_input)
		:
			Root_component(ep, md_alloc),
			_usb_input(usb_input), _ps2_input(ps2_input)
		{ }

		Input::Session_component *_create_session(const char *args,
		                                          Affinity const &)
		{
			Session_label const label = label_from_args(args);

			if (label.last_element() == "usb") return &_usb_input;
			if (label.last_element() == "ps2") return &_ps2_input;

			error("no matching policy for session label ", label);
			throw Root::Invalid_args();
		}

		/*
		 * Prevent the default 'Root_component' implementation from attempting
		 * to free the session objects.
		 */
		void _destroy_session(Input::Session_component *) override { }
};


class Test::Input_to_filter
{
	private:

		Env &_env;

		Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

		/*
		 * Provide the input service via an independent entrypoint to avoid a
		 * possible deadlock between the input_filter and the test when
		 * both try to invoke 'Input::Session::flush' from each other.
		 */
		enum { STACK_SIZE = 4*1024*sizeof(long) };

		Entrypoint _ep { _env, STACK_SIZE, "input_server_ep" };

		/*
		 * Input supplied to the input_filter
		 */
		Input::Session_component _usb { _env, _env.ram() };
		Input::Session_component _ps2 { _env, _env.ram() };

		Input_root _root { _ep, _sliced_heap, _usb, _ps2};

		typedef String<20> Key_name;

		Input::Keycode _code(Key_name const &key_name)
		{
			for (unsigned i = 0; i < Input::KEY_MAX - 1; i++) {
				Input::Keycode const code = Input::Keycode(i);
				if (key_name == Input::key_name(code))
					return code;
			}

			error("unknown key name: ", key_name);
			throw Exception();
		};

	public:

		Input_to_filter(Env &env) : _env(env)
		{
			_env.parent().announce(_ep.manage(_root));

			_usb.event_queue().enabled(true);
			_ps2.event_queue().enabled(true);
		}

		void submit_events(Xml_node step)
		{
			if (step.type() != "usb" && step.type() != "ps2") {
				error("unexpected argument to Input_to_filter::submit");
				throw Exception();
			}

			Input::Session_component &dst = step.type() == "usb" ? _usb : _ps2;

			step.for_each_sub_node([&] (Xml_node node) {

				Input::Event::Type const type =
					node.type() == "press"   ? Input::Event::PRESS :
					node.type() == "release" ? Input::Event::RELEASE :
					                           Input::Event::INVALID;

				if (type == Input::Event::PRESS || type == Input::Event::RELEASE) {
					Key_name const key_name = node.attribute_value("code", Key_name());
					dst.submit(Input::Event(type, _code(key_name), 0, 0, 0, 0));
				}
			});
		}
};


struct Test::Main : Input_from_filter::Event_handler
{
	Env &_env;

	Timer::Connection _timer { _env };

	Input_from_filter _input_from_filter { _env, *this };

	Input_to_filter _input_to_filter { _env };

	Reporter _input_filter_config_reporter { _env, "config",  "input_filter.config" };
	Reporter _chargen_include_reporter     { _env, "chargen", "chargen_include" };
	Reporter _remap_include_reporter       { _env, "remap",   "remap_include" };

	Attached_rom_dataspace _config { _env, "config" };

	void _publish_report(Reporter &reporter, Xml_node node)
	{
		Reporter::Xml_generator xml(reporter, [&] () {
			xml.append(node.content_base(), node.content_size()); });
	}

	void _gen_chargen_rec(Xml_generator &xml, unsigned depth)
	{
		if (depth > 0) {
			xml.node("chargen", [&] () { _gen_chargen_rec(xml, depth - 1); });
		} else {
			xml.node("input", [&] () { xml.attribute("name", "usb"); });
		}
	}

	void _deep_filter_config(Reporter &reporter, Xml_node node)
	{
		unsigned const depth = node.attribute_value("depth", 0UL);

		Reporter::Xml_generator xml(_input_filter_config_reporter, [&] () {
			xml.node("input",  [&] () { xml.attribute("label", "usb"); });
			xml.node("output", [&] () { _gen_chargen_rec(xml, depth); });
		});
	}

	unsigned const _num_steps = _config.xml().num_sub_nodes();
	unsigned       _curr_step = 0;

	unsigned long _went_to_sleep_time = 0;

	Xml_node _curr_step_xml() const { return _config.xml().sub_node(_curr_step); }

	void _advance_step()
	{
		_curr_step++;

		/* exit when reaching the end of the sequence */
		if (_curr_step == _num_steps) {
			_env.parent().exit(0);
			sleep_forever();
		}
	};

	void _execute_curr_step()
	{
		for (;;) {
			Xml_node const step = _curr_step_xml();

			log("step ", _curr_step, " (", step.type(), ")");

			_input_from_filter.input_expected(step.type() == "expect_press"   ||
			                                  step.type() == "expect_release" ||
			                                  step.type() == "expect_char");

			if (step.type() == "filter_config") {
				_publish_report(_input_filter_config_reporter, step);
				_advance_step();
				continue;
			}

			if (step.type() == "deep_filter_config") {
				_deep_filter_config(_input_filter_config_reporter, step);
				_advance_step();
				continue;
			}

			if (step.type() == "chargen_include") {
				_publish_report(_chargen_include_reporter, step);
				_advance_step();
				continue;
			}

			if (step.type() == "remap_include") {
				_publish_report(_remap_include_reporter, step);
				_advance_step();
				continue;
			}

			if (step.type() == "usb" || step.type() == "ps2") {
				_input_to_filter.submit_events(step);
				_advance_step();
				continue;
			}

			if (step.type() == "message") {
				typedef String<80> Message;
				Message const message = step.attribute_value("string", Message());
				log("\n--- ", message, " ---");
				_advance_step();
				continue;
			}

			if (step.type() == "nop") {
				_advance_step();
				continue;
			}

			if (step.type() == "expect_press" || step.type() == "expect_release"
			 || step.type() == "expect_char")
				return;

			if (step.type() == "sleep") {
				if (_went_to_sleep_time == 0) {
					unsigned long const timeout_ms = step.attribute_value("ms", 250UL);
					_went_to_sleep_time = _timer.elapsed_ms();
					_timer.trigger_once(timeout_ms*1000);
				}
				return;
			}

			error("unexpected step: ", step);
			throw Exception();
		}
	}

	/**
	 * Input_to_filter::Event_handler interface
	 */
	void handle_event_from_filter(Input::Event const &ev) override
	{
		typedef Genode::String<20> Value;

		Xml_node const step = _curr_step_xml();

		switch (ev.type()) {
		case Input::Event::PRESS:
			if (step.type() == "expect_press"
			 && step.attribute_value("code", Value()) == Input::key_name(ev.keycode()))
				break;

		case Input::Event::RELEASE:
			if (step.type() == "expect_release"
			 && step.attribute_value("code", Value()) == Input::key_name(ev.keycode()))
				break;

		case Input::Event::CHARACTER:
			if (step.type() == "expect_char"
			 && step.attribute_value("char", Value()) == Value(Char(ev.utf8().b0)))
			 	break;

		case Input::Event::INVALID:
		case Input::Event::MOTION:
		case Input::Event::WHEEL:
		case Input::Event::FOCUS:
		case Input::Event::LEAVE:
		case Input::Event::TOUCH:
			error("unexpected event: ", ev);
			throw Exception();
		};

		_advance_step();
		_execute_curr_step();
	}

	void _handle_timer()
	{
		if (_curr_step_xml().type() != "sleep") {
			error("got spurious timeout signal");
			throw Exception();
		}

		unsigned long const duration = _curr_step_xml().attribute_value("ms", 0UL);
		unsigned long const now      = _timer.elapsed_ms();
		unsigned long const slept    = now - _went_to_sleep_time;

		if (slept < duration) {
			warning("spurious wakeup from sleep");
			_timer.trigger_once(1000*(duration - slept));
			return;
		}

		/* skip <sleep> */
		_advance_step();

		_went_to_sleep_time = 0;
		_execute_curr_step();
	}

	Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	Main(Env &env) : _env(env)
	{
		_timer.sigh(_timer_handler);
		_input_filter_config_reporter.enabled(true);
		_chargen_include_reporter.enabled(true);
		_remap_include_reporter.enabled(true);
		_execute_curr_step();
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

