/*
 * \brief  Test for the init component
 * \author Norman Feske
 * \date   2017-02-16
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
#include <timer_session/connection.h>
#include <log_session/log_session.h>
#include <root/component.h>
#include <os/reporter.h>
#include <base/sleep.h>

namespace Test {

	struct Log_message_handler;
	class  Log_session_component;
	class  Log_root;
	struct Main;

	using namespace Genode;

	static bool xml_attribute_matches(Xml_node, Xml_node);
	static bool xml_matches(Xml_node, Xml_node);
}


/***************
 ** Utilities **
 ***************/

static inline bool Test::xml_attribute_matches(Xml_node condition, Xml_node node)
{
	typedef String<32> Name;
	typedef String<64> Value;

	Name const name = condition.attribute_value("name",  Name());

	if (condition.has_attribute("value")) {
		Value const value = condition.attribute_value("value", Value());
		return node.attribute_value(name.string(), Value()) == value;
	}

	if (condition.has_attribute("higher")) {
		size_t const value = condition.attribute_value("higher", Number_of_bytes());
		return (size_t)node.attribute_value(name.string(), Number_of_bytes()) > value;
	}

	if (condition.has_attribute("lower")) {
		size_t const value = condition.attribute_value("lower", Number_of_bytes());
		return (size_t)node.attribute_value(name.string(), Number_of_bytes()) < value;
	}

	error("missing condition in <attribute> node");
	return false;
}


/**
 * Return true if 'node' has expected content
 *
 * \expected  description of the XML content expected in 'node'
 */
static inline bool Test::xml_matches(Xml_node expected, Xml_node node)
{
	bool matches = true;
	expected.for_each_sub_node([&] (Xml_node condition) {

		if (condition.type() == "attribute")
			matches = matches && xml_attribute_matches(condition, node);

		if (condition.type() == "node") {

			typedef String<32> Name;
			Name const name = condition.attribute_value("name", Name());

			bool at_least_one_sub_node_matches = false;
			node.for_each_sub_node(name.string(), [&] (Xml_node sub_node) {
				if (xml_matches(condition, sub_node))
					at_least_one_sub_node_matches = true; });

			matches = matches && at_least_one_sub_node_matches;
		}

		if (condition.type() == "not")
			matches = matches && !xml_matches(condition, node);
	});
	return matches;
}


struct Test::Log_message_handler : Interface
{
	typedef String<Log_session::MAX_STRING_LEN> Message;

	enum Result { EXPECTED, UNEXPECTED, IGNORED };

	virtual Result handle_log_message(Message const &message) = 0;
};


namespace Genode
{
	static inline void print(Output &output, Test::Log_message_handler::Result result)
	{
		using Genode::print;
		switch (result) {
		case Test::Log_message_handler::EXPECTED:   print(output, "expected"); break;
		case Test::Log_message_handler::UNEXPECTED: print(output, "expected"); break;
		case Test::Log_message_handler::IGNORED:    print(output, "ignored");  break;
		}
	}
}


class Test::Log_session_component : public Rpc_object<Log_session>
{
	private:

		Session_label const _label;

		Log_message_handler &_handler;

	public:

		Log_session_component(Session_label const &label, Log_message_handler &handler)
		:
			_label(label), _handler(handler)
		{ }

		void write(String const &string) override
		{
			/* strip known line delimiter from incoming message */
			unsigned n = 0;
			static char const delim[] = "\033[0m\n";
			static Genode::String<sizeof(delim)> const pattern(delim);
			for (char const *s = string.string(); s[n] && pattern != s + n; n++);

			Log_message_handler::Message const
				message("[", _label, "] ", Cstring(string.string(), n));

			Log_message_handler::Result const result =
				_handler.handle_log_message(message);

			log(message, " (", result, ")");
		}
};


class Test::Log_root : public Root_component<Log_session_component>
{
	private:

		Log_message_handler &_handler;

	public:

		Log_root(Entrypoint &ep, Allocator &md_alloc, Log_message_handler &handler)
		:
			Root_component(ep, md_alloc), _handler(handler)
		{ }

		Log_session_component *_create_session(const char *args, Affinity const &) override
		{
			Session_label const label = label_from_args(args);

			return new (md_alloc()) Log_session_component(label, _handler);
		}
};


struct Test::Main : Log_message_handler
{
	Env &_env;

	Timer::Connection _timer { _env };

	bool _timer_scheduled = false;

	Reporter _init_config_reporter { _env, "config",  "init.config" };

	Attached_rom_dataspace _config { _env, "config" };

	void _publish_report(Reporter &reporter, Xml_node node)
	{
		typedef String<64> Version;
		Version const version = node.attribute_value("version", Version());

		Reporter::Xml_generator xml(reporter, [&] () {

			if (version.valid())
				xml.attribute("version", version);

			node.with_raw_content([&] (char const *start, size_t length) {
				xml.append(start, length); });
		});
	}

	unsigned const _num_steps = _config.xml().num_sub_nodes();
	unsigned       _curr_step = 0;

	Xml_node _curr_step_xml() const { return _config.xml().sub_node(_curr_step); }

	/*
	 * Handling of state reports generated by init
	 */
	Attached_rom_dataspace _init_state { _env, "state" };

	Signal_handler<Main> _init_state_handler {
		_env.ep(), *this, &Main::_handle_init_state };

	void _handle_init_state()
	{
		_init_state.update();
		_execute_curr_step();
	}

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

			if (step.type() == "expect_log") {
				_expect_log_msg = _curr_step_xml().attribute_value("string", Log_message_handler::Message());
				_expect_log     = true;
				return;
			}
			if (step.type() == "expect_warning") {
				_expect_log_msg = _curr_step_xml().attribute_value("string", Log_message_handler::Message());
				Log_message_handler::Message colored (_curr_step_xml().attribute_value("colored", Log_message_handler::Message()));
				_expect_log_msg = Log_message_handler::Message(_expect_log_msg,
				                                               "\033[34m",
				                                               colored);
				_expect_log     = true;
				return;
			}

			if (step.type() == "expect_init_state") {
				if (xml_matches(step, _init_state.xml())) {
					_advance_step();
					continue;
				} else {
					warning("init state does not match: ", _init_state.xml());
					warning("expected condition: ", step);
				}
				return;
			}

			if (step.type() == "init_config") {
				_publish_report(_init_config_reporter, step);
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

			if (step.type() == "sleep") {
				if (!_timer_scheduled) {
					uint64_t const timeout_ms = step.attribute_value("ms", (uint64_t)250);
					_timer.trigger_once(timeout_ms*1000);
					_timer_scheduled = true;
				}
				return;
			}

			error("unexpected step: ", step);
			throw Exception();
		}
	}

	/**
	 * Log_message_handler interface
	 */
	Result handle_log_message(Log_message_handler::Message const &message) override
	{
		if (!_expect_log)
			return IGNORED;

		if (message != _expect_log_msg)
			return IGNORED;

		_expect_log = false;
		_advance_step();
		_execute_curr_step();
		return EXPECTED;
	}

	/*
	 * Timer handling
	 */
	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

	void _handle_timer()
	{
		if (_curr_step_xml().type() != "sleep") {
			error("got spurious timeout signal");
			throw Exception();
		}

		_timer_scheduled = false;

		_advance_step();
		_execute_curr_step();
	}

	/*
	 * LOG service provided to init
	 */
	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Log_root _log_root { _env.ep(), _sliced_heap, *this };

	bool                         _expect_log = false;
	Log_message_handler::Message _expect_log_msg { };


	Main(Env &env) : _env(env)
	{
		_timer.sigh(_timer_handler);
		_init_config_reporter.enabled(true);
		_init_state.sigh(_init_state_handler);
		_execute_curr_step();

		_env.parent().announce(_env.ep().manage(_log_root));
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

