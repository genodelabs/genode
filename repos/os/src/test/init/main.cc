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

	static bool node_attribute_matches(Node const &, Node const &);
	static bool node_matches(Node const &, Node const &);
}


/***************
 ** Utilities **
 ***************/

static inline bool Test::node_attribute_matches(Node const &condition,
                                                Node const &node)
{
	using Name  = String<32>;
	using Value = String<64>;

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
 * \expected  description of the node content expected in 'node'
 */
static inline bool Test::node_matches(Node const &expected, Node const &node)
{
	bool matches = true;
	expected.for_each_sub_node([&] (Node const &condition) {

		if (condition.type() == "attribute")
			matches = matches && node_attribute_matches(condition, node);

		if (condition.type() == "node") {

			using Name = String<32>;
			Name const name = condition.attribute_value("name", Name());

			bool at_least_one_sub_node_matches = false;
			node.for_each_sub_node(name.string(), [&] (Node const &sub_node) {
				if (node_matches(condition, sub_node))
					at_least_one_sub_node_matches = true; });

			matches = matches && at_least_one_sub_node_matches;
		}

		if (condition.type() == "not")
			matches = matches && !node_matches(condition, node);
	});
	return matches;
}


struct Test::Log_message_handler : Interface
{
	using Message = String<Log_session::MAX_STRING_LEN>;

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

		Create_result _create_session(const char *args, Affinity const &) override
		{
			Session_label const label = label_from_args(args);

			return *new (md_alloc()) Log_session_component(label, _handler);
		}
};


struct Test::Main : Log_message_handler
{
	Env &_env;

	Timer::Connection _timer { _env };

	bool _timer_scheduled = false;

	Reporter _init_config_reporter { _env, "config",  "init.config" };

	Attached_rom_dataspace _config { _env, "config" };

	void _publish_report(Reporter &reporter, Node const &node)
	{
		using Version = String<64>;
		Version const version = node.attribute_value("version", Version());

		(void)reporter.generate([&] (Xml_generator &xml) {

			if (version.valid())
				xml.attribute("version", version);

			node.for_each_sub_node([&] (Node const &sub_node) {
				(void)xml.append_node(sub_node, { 20 }); }); });
	}

	unsigned const _num_steps = (unsigned)_config.node().num_sub_nodes();
	unsigned       _curr_step = 0;

	void _with_curr_step_node(auto const &fn) const
	{
		_config.node().with_sub_node(_curr_step, fn, [] { });
	}

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
		enum Result { CONTINUE, RETURN };

		auto execute_step = [&] (Node const &step)
		{
			log("step ", _curr_step, " (", step.type(), ")");

			if (step.type() == "expect_log") {
				_expect_log_msg = step.attribute_value("string", Log_message_handler::Message());
				_expect_log     = true;
				return RETURN;
			}
			if (step.type() == "expect_warning") {
				_expect_log_msg = step.attribute_value("string", Log_message_handler::Message());
				Log_message_handler::Message colored (step.attribute_value("colored", Log_message_handler::Message()));
				_expect_log_msg = Log_message_handler::Message(_expect_log_msg,
				                                               "\033[34m",
				                                               colored);
				_expect_log     = true;
				return RETURN;
			}

			if (step.type() == "expect_init_state") {
				if (node_matches(step, _init_state.node())) {
					_advance_step();
					return CONTINUE;
				} else {
					warning("init state does not match: ", _init_state.node());
					warning("expected condition: ", step);
				}
				return RETURN;
			}

			if (step.type() == "init_config") {
				_publish_report(_init_config_reporter, step);
				_advance_step();
				return CONTINUE;
			}

			if (step.type() == "message") {
				using Message = String<80>;
				Message const message = step.attribute_value("string", Message());
				log("\n--- ", message, " ---");
				_advance_step();
				return CONTINUE;
			}

			if (step.type() == "nop") {
				_advance_step();
				return CONTINUE;
			}

			if (step.type() == "sleep") {
				if (!_timer_scheduled) {
					uint64_t const timeout_ms = step.attribute_value("ms", (uint64_t)250);
					_timer.trigger_once(timeout_ms*1000);
					_timer_scheduled = true;
				}
				return RETURN;
			}

			error("unexpected step: ", step);
			throw Exception();
		};

		for (Result result = CONTINUE; result == CONTINUE; ) {
			result = RETURN;
			_with_curr_step_node([&] (Node const &step) {
				result = execute_step(step); });
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
		_with_curr_step_node([&] (Node const &step) {
			if (step.type() != "sleep") {
				error("got spurious timeout signal");
				throw Exception();
			}
		});

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

