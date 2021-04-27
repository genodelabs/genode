/*
 * \brief  Test for event filter
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
#include <base/attached_ram_dataspace.h>
#include <event_session/connection.h>
#include <base/session_object.h>
#include <timer_session/connection.h>
#include <root/component.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <os/reporter.h>
#include <os/ring_buffer.h>
#include <base/sleep.h>

namespace Test {
	class Input_from_filter;
	class Input_to_filter;
	class Input_root;
	class Event_session;
	class Event_root;
	class Main;
	using namespace Genode;
}


struct Test::Event_session : Rpc_object<Event::Session, Event_session>
{
	Attached_ram_dataspace _ds;

	Signal_context_capability _handle_input_sigh;

	Ring_buffer<Input::Event, 100> _events { };

	Event_session(Env &env, Signal_context_capability handle_input_sigh)
	:
		_ds(env.ram(), env.rm(), 4096), _handle_input_sigh(handle_input_sigh)
	{ }

	template <typename FN>
	void for_each_pending_event(FN const &fn)
	{
		while (!_events.empty())
			fn(_events.get());
	}


	/*****************************
	 ** Event session interface **
	 *****************************/

	Dataspace_capability dataspace() { return _ds.cap(); }

	void submit_batch(unsigned count)
	{
		size_t const max_events = _ds.size() / sizeof(Input::Event);

		if (count > max_events)
			warning("number of events exceeds dataspace capacity");

		count = min(count, max_events);

		Input::Event const * const events = _ds.local_addr<Input::Event>();

		for (unsigned i = 0; i < count; i++) {

			if (_events.avail_capacity() < 1)
				error("ring-buffer overflow");

			_events.add(events[i]);
		}

		/* execute '_handle_input' in the context of the main entrypoint */
		Signal_transmitter(_handle_input_sigh).submit();
	}
};


struct Test::Event_root : Root_component<Event_session>
{
	Event_session &_session;

	bool _filter_connected = false;

	Event_root(Entrypoint &ep, Allocator &md_alloc, Event_session &session)
	:
		Root_component(ep, md_alloc), _session(session)
	{ }

	Event_session *_create_session(const char *, Affinity const &) override
	{
		_filter_connected = true;

		return &_session;
	}

	/*
	 * Prevent the default 'Root_component' implementation from attempting
	 * to free the session objects.
	 */
	void _destroy_session(Event_session *) override { }
};


class Test::Input_from_filter
{
	public:

		struct Event_handler : Interface
		{
			virtual void handle_event_from_filter(Input::Event const &) = 0;
		};

	private:

		Env &_env;

		Event_handler &_event_handler;

		bool _input_expected = false;

		bool _handle_input_in_progress = false;

		Signal_handler<Input_from_filter> _input_handler {
			_env.ep(), *this, &Input_from_filter::_handle_input };

		Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

		/*
		 * Provide the event service via an independent entrypoint to avoid a
		 * possible deadlock between the event_filter and the test when
		 * both try to invoke 'Event::Session::submit' from each other.
		 */
		enum { STACK_SIZE = 4*1024*sizeof(long) };

		Entrypoint _ep { _env, STACK_SIZE, "server_ep", Affinity::Location() };

		Event_session _session { _env, _input_handler };

		Event_root _root { _ep, _sliced_heap, _session };

		void _handle_input()
		{
			_handle_input_in_progress = true;

			if (_input_expected)
				_session.for_each_pending_event([&] (Input::Event const &event) {
					_event_handler.handle_event_from_filter(event); });

			_handle_input_in_progress = false;
		}

	public:

		Input_from_filter(Env &env, Event_handler &event_handler)
		:
			_env(env), _event_handler(event_handler)
		{
			_env.parent().announce(_ep.manage(_root));
		}

		void input_expected(bool expected)
		{
			_input_expected = expected;

			/* prevent nested call of '_handle_input' */
			if (!_input_expected || _handle_input_in_progress)
				return;

			/* if new step expects input, process currently pending events */
			_handle_input();
		}
};


class Test::Input_to_filter
{
	private:

		Env &_env;

		Constructible<Event::Connection> _ps2 { };
		Constructible<Event::Connection> _usb { };

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

		Input_to_filter(Env &env) : _env(env) { }

		void apply_driver(Xml_node driver)
		{
			using Name = String<100>;
			Name const name = driver.attribute_value("name", Name());

			bool const connected = driver.attribute_value("connected", true);

			if (name == "ps2") _ps2.conditional(connected, _env, "ps2");
			if (name == "usb") _usb.conditional(connected, _env, "usb");
		}

		void submit_events(Xml_node step)
		{
			if (step.type() != "usb" && step.type() != "ps2") {
				error("unexpected argument to Input_to_filter::submit");
				throw Exception();
			}

			Event::Connection &dst = step.type() == "usb" ? *_usb : *_ps2;

			dst.with_batch([&] (Event::Session_client::Batch &batch) {

				step.for_each_sub_node([&] (Xml_node node) {

					bool const press   = node.has_type("press"),
					           release = node.has_type("release");

					if (press || release) {

						Key_name const key_name = node.attribute_value("code", Key_name());

						if (press)   batch.submit(Input::Press  {_code(key_name)});
						if (release) batch.submit(Input::Release{_code(key_name)});
					}

					bool const motion = node.has_type("motion");
					bool const rel = node.has_attribute("rx") || node.has_attribute("ry");
					bool const abs = node.has_attribute("ax") || node.has_attribute("ay");

					if (motion && abs)
						batch.submit(Input::Absolute_motion{(int)node.attribute_value("ax", 0L),
						                                    (int)node.attribute_value("ay", 0L)});

					if (motion && rel)
						batch.submit(Input::Relative_motion{(int)node.attribute_value("rx", 0L),
						                                    (int)node.attribute_value("ry", 0L)});
				});
			});
		}
};


struct Test::Main : Input_from_filter::Event_handler
{
	Env &_env;

	Timer::Connection _timer { _env };

	Input_from_filter _input_from_filter { _env, *this };

	Input_to_filter _input_to_filter { _env };

	Reporter _event_filter_config_reporter { _env, "config",   "event_filter.config" };
	Reporter _chargen_include_reporter     { _env, "chargen",  "chargen_include" };
	Reporter _remap_include_reporter       { _env, "remap",    "remap_include" };
	Reporter _capslock_reporter            { _env, "capslock", "capslock" };

	Attached_rom_dataspace _config { _env, "config" };

	void _publish_report(Reporter &reporter, Xml_node node)
	{
		Reporter::Xml_generator xml(reporter, [&] () {
			node.with_raw_content([&] (char const *start, size_t length) {
				xml.append(start, length); }); });
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

		Reporter::Xml_generator xml(reporter, [&] () {
			xml.node("input",  [&] () { xml.attribute("label", "usb"); });
			xml.node("output", [&] () { _gen_chargen_rec(xml, depth); });
		});
	}

	unsigned const _num_steps = _config.xml().num_sub_nodes();
	unsigned       _curr_step = 0;

	uint64_t _went_to_sleep_time = 0;

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
			                                  step.type() == "not_expect_press" ||
			                                  step.type() == "not_expect_release" ||
			                                  step.type() == "expect_char"    ||
			                                  step.type() == "expect_motion"  ||
			                                  step.type() == "expect_wheel");

			if (step.type() == "driver") {
				_input_to_filter.apply_driver(step);
				_advance_step();
				continue;
			}

			if (step.type() == "filter_config") {
				_publish_report(_event_filter_config_reporter, step);
				_advance_step();
				continue;
			}

			if (step.type() == "deep_filter_config") {
				_deep_filter_config(_event_filter_config_reporter, step);
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

			if (step.type() == "capslock") {
				Reporter::Xml_generator xml(_capslock_reporter, [&] () {
					xml.attribute("enabled", step.attribute_value("enabled", false)); });
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
			 || step.type() == "not_expect_press" || step.type() == "not_expect_release"
			 || step.type() == "expect_char"  || step.type() == "expect_motion"
			 || step.type() == "expect_wheel")
				return;

			if (step.type() == "sleep") {
				if (_went_to_sleep_time == 0) {
					uint64_t const timeout_ms = step.attribute_value("ms", (uint64_t)250);
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

		bool step_succeeded = false;
		bool step_failed    = false;

		ev.handle_press([&] (Input::Keycode key, Codepoint codepoint) {

			auto codepoint_of_step = [] (Xml_node step) {
				if (step.has_attribute("codepoint"))
					return Codepoint { step.attribute_value("codepoint", 0U) };
				return Utf8_ptr(step.attribute_value("char", Value()).string()).codepoint();
			};

			if (step.type() == "expect_press"
			 && step.attribute_value("code", Value()) == Input::key_name(key)
			 && ((!step.has_attribute("char") && !step.has_attribute("codepoint")) ||
			     codepoint_of_step(step).value == codepoint.value))
				step_succeeded = true;

			if (step.type() == "not_expect_press") {
				if (step.attribute_value("code", Value()) == Input::key_name(key)
			    && ((!step.has_attribute("char") && !step.has_attribute("codepoint")) ||
			        codepoint_of_step(step).value == codepoint.value))
					step_failed = true;
				else
					step_succeeded = true;
			}
		});

		ev.handle_release([&] (Input::Keycode key) {
			if (step.type() == "expect_release"
			 && step.attribute_value("code", Value()) == Input::key_name(key))
				step_succeeded = true;

			if (step.type() == "not_expect_release") {
				if (step.attribute_value("code", Value()) == Input::key_name(key))
					step_failed = true;
				else
					step_succeeded = true;
			}

		});

		ev.handle_wheel([&] (int x, int y) {
			if (step.type() == "expect_wheel"
			 && step.attribute_value("rx", 0L) == x
			 && step.attribute_value("ry", 0L) == y)
				step_succeeded = true; });

		ev.handle_relative_motion([&] (int x, int y) {
			if (step.type() == "expect_motion"
			 && (!step.has_attribute("rx") || step.attribute_value("rx", 0L) == x)
			 && (!step.has_attribute("ry") || step.attribute_value("ry", 0L) == y))
				step_succeeded = true; });

		ev.handle_absolute_motion([&] (int x, int y) {
			if (step.type() == "expect_motion"
			 && (!step.has_attribute("ax") || step.attribute_value("ax", 0L) == x)
			 && (!step.has_attribute("ay") || step.attribute_value("ay", 0L) == y))
				step_succeeded = true; });

		if (step_failed) {
			error("got unexpected event: ", step);
			throw Exception();
		}

		if (step_succeeded) {
			_advance_step();
			_execute_curr_step();
		}
	}

	void _handle_timer()
	{
		if (_curr_step_xml().type() != "sleep") {
			error("got spurious timeout signal");
			throw Exception();
		}

		uint64_t const duration = _curr_step_xml().attribute_value("ms", (uint64_t)0);
		uint64_t const now      = _timer.elapsed_ms();
		uint64_t const slept    = now - _went_to_sleep_time;

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
		_event_filter_config_reporter.enabled(true);
		_chargen_include_reporter.enabled(true);
		_remap_include_reporter.enabled(true);
		_capslock_reporter.enabled(true);
		_execute_curr_step();
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

