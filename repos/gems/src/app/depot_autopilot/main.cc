/*
 * \brief  Orchestrator of automated tests
 * \author Norman Feske
 * \date   2025-11-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <os/reporter.h>
#include <util/formatted_output.h>

/* local includes */
#include <plan.h>
#include <log_session.h>

namespace Depot_autopilot {
	struct Iteration;
	struct Main;
}


struct Depot_autopilot::Iteration
{
	Env &_env;

	Heap &_heap;

	struct Action : Interface, Noncopyable
	{
		virtual Clock now() = 0;
		virtual void single_test_done() = 0;
		virtual void schedule_timeout(Clock) = 0;
	};

	Action &_action;

	struct Attr { Clock total_start_time; };

	Attr const _attr;

	Log_buffer _log_buffer { _heap };

	Plan _plan { };

	Expanding_reporter _deploy_reporter { _env, "deploy", "deploy"};

	void _conclude_single_test(Test &test)
	{
		log("");
		test.print_conclusion();
		_action.single_test_done();
	}

	void _try_run_test(Clock now)
	{
		_plan.with_current_test([&] (Test &test) {

			if (test.running())
				return;

			/* wait for blue print */
			if (!test.skip && !test.malformed && !test.blueprint_defined)
				return;

			log("\n--- Run \"", test.name, "\" (max ", test.max_seconds(), " sec) ---\n");
			test.start_time = test.end_time = now;

			if (test.skip) {
				test.state = Test::State::SKIPPED;
				_conclude_single_test(test);
				return;
			}

			bool well_defined = true;

			test.deadline().with_result(
				[&] (Clock timeout) { _action.schedule_timeout(timeout); },
				[&] (Test::Deadline_error e) {
					if (e == Test::Deadline_error::AMBIGIOUS)
						error(test.name, " has ambigious timeouts defined");
					if (e == Test::Deadline_error::MISSING)
						error(test.name, " has no timeout defined");
					well_defined = false;
				});

			if (!well_defined) {
				test.state = Test::State::FAILED;
				test.malformed  = true;
				_conclude_single_test(test);
				return;
			}

			test.state = Test::State::RUNNING;
		});
	}

	Iteration(Env &env, Heap &heap, Action &action, Attr attr)
	:
		_env(env), _heap(heap), _action(action), _attr(attr)
	{ }

	~Iteration()
	{
		_plan.print_conclusions();
		_plan.apply_config(_heap, Node());
	}

	void select_next_test_if_idle()
	{
		if (_plan.any_running() || _plan.all_done())
			return;

		Clock const now = _action.now();

		_log_buffer.reset();

		_plan.select_next_scheduled();
		_try_run_test(now);
	}

	bool all_done() const { return _plan.all_done(); }

	void apply_config_and_blueprint(Node const &config, Node const &blueprint)
	{
		_plan.apply_config   (_heap, config);
		_plan.apply_blueprint(_heap, blueprint);

		/* unblock selected test waiting for its blueprint */
		_plan.with_selected_test_not_yet_running([&] (Test &test) {
			if (test.blueprint_defined)
				_try_run_test(_action.now()); });
	}

	void reconfigure_deploy()
	{
		_deploy_reporter.generate([&] (Generator &g) {
			_plan.gen_deploy_start_nodes(g); });
	}

	Stats stats(Clock now, Node const &config) const
	{
		Stats stats = _plan.stats();

		stats.total_time = { .ms = now.ms - _attr.total_start_time.ms };

		config.with_optional_sub_node("previous-results", [&] (Node const &node) {
			stats.total_time.ms += node.attribute_value("time_sec",  0u)*1000;
			stats.succeeded     += node.attribute_value("succeeded", 0U);
			stats.failed        += node.attribute_value("failed",    0U);
			stats.skipped       += node.attribute_value("skipped",   0U);
		});
		return  stats;
	}

	void handle_log_message_for_current_test(Clock const now, Span const &msg)
	{
		_log_buffer.append(msg);

		_plan.with_running_test([&] (Test &test) {
			test.evaluate_log(now, _log_buffer);
			if (!test.running())
				_conclude_single_test(test); });
	}

	void handle_timeout_for_current_test(Clock const now)
	{
		_plan.with_running_test([&] (Test &test) {
			test.evaluate_timeout(now);
			if (!test.running())
				_conclude_single_test(test); });
	}

	void handle_exit_of_current_test(Clock const now, Exit const exit)
	{
		_plan.with_running_test([&] (Test &test) {
			test.evaluate_exit(now, exit.code);
			if (!test.running())
				_conclude_single_test(test); });
	}
};


struct Depot_autopilot::Main : Log_session::Action, Iteration::Action
{
	Env &_env;

	Attached_rom_dataspace _config    { _env, "config" },
	                       _blueprint { _env, "blueprint" },
	                       _state     { _env, "state" };

	Timer::Connection _timer { _env };

	Clock const _start_time { .ms = _timer.elapsed_ms() };

	Session_label const _runtime_prefix =
		_config.node().attribute_value("children_label_prefix", String<160>());

	Constructible<Iteration> _iteration { };

	Heap _heap { _env.ram(), _env.rm() };

	Log_root _log_root { _env.ep(), _heap, *this };

	Signal_handler<Main>
		_config_handler         { _env.ep(), *this, &Main::_handle_config },
		_iteration_done_handler { _env.ep(), *this, &Main::_handle_iteration_done },
		_timeout_handler        { _env.ep(), *this, &Main::_handle_timeout },
		_runtime_state_handler  { _env.ep(), *this, &Main::_handle_runtime_state };

	/**
	 * Log_session::Action interface
	 */
	void handle_log_message(Span const &origin, Span const &msg) override
	{
		if (!_iteration.constructed() || !_iteration->_plan.any_running()) {
			warning("spurious log message: '", Cstring(msg.start, msg.num_bytes), "'");
			return;
		}

		auto with_skipped_prefix = [] (Span const &bytes, Span const &prefix, auto const &fn)
		{
			auto with_skipped = [] (Span const &bytes, size_t n, auto const &fn)
			{
				n = min(n, bytes.num_bytes);
				fn(Span { bytes.start + n, bytes.num_bytes - n });
			};

			with_skipped(bytes, prefix.num_bytes, [&] (Span const &bytes) {
				Span const sep { " -> ", 4 };
				if (bytes.starts_with(sep))
					with_skipped(bytes, sep.num_bytes, [&] (Span const &bytes) {
						fn(bytes); });
				else
					fn(bytes);
			});
		};

		_iteration->_plan.with_running_test([&] (Test &test) {

			Clock const now { _timer.elapsed_ms() };

			auto capture_line = [&] (Span const &origin, Span const &line)
			{
				if (!test.running()) /* stop capturing after success of previous span */
					return;

				using Tag = String<128>;
				Tag const tag = origin.num_bytes
				              ? Tag("[", Cstring(origin.start, origin.num_bytes), "] ")
				              : Tag();

				using Msg = String<512>;
				Msg const msg(tag, Cstring(line.start, line.num_bytes));

				log(Right_aligned { 5, Clock { now.ms - test.start_time.ms } }, " ", msg);

				/* match start of line against start of pattern line, marked by '\n' */
				Msg("\n", msg).with_span([&] (Span const &span) {
					_iteration->handle_log_message_for_current_test(now, span); });
			};

			Session_label const expected_prefix = prefixed_label(_runtime_prefix, test.name);

			expected_prefix.with_span([&] (Span const &prefix) {
				if (!origin.starts_with(prefix)) {
					warning("spurious log message by '", Cstring(origin.start, origin.num_bytes), "'");
					return;
				}
				with_skipped_prefix(origin, prefix, [&] (Span const &origin) {
					if (msg.num_bytes)
						msg.split('\n', [&] (Span const &line) { capture_line(origin, line); });
					else
						capture_line(origin, Span { nullptr, 0u } /* empty line */ );
				});
			});
		});
	}

	/**
	 * Log_session::Action interface
	 */
	Test::Name curr_test_name() override
	{
		Test::Name result { };

		if (_iteration.constructed())
			_iteration->_plan.with_running_test([&] (Test &t) { result = t.name; });

		if (result.length() <= 1)
			warning("LOG session requested while no test is running");

		return result;
	}

	/**
	 * Iteration::Action interface
	 */
	Clock now() override { return { .ms = _timer.elapsed_ms() }; }

	/**
	 * Iteration::Action interface
	 */
	void single_test_done() override
	{
		if (!_iteration.constructed()) {
			warning("single_test_done unexpectedly called without iteration");
			return;
		}

		_log_root.current_session_done();
		_iteration->select_next_test_if_idle();
		_iteration->reconfigure_deploy();

		if (_iteration->all_done())
			_iteration_done_handler.local_submit();
	}

	/**
	 * Iteration::Action interface
	 */
	void schedule_timeout(Clock abs_timeout) override
	{
		Clock const now = this->now();

		uint64_t const rel_ms = abs_timeout.ms - min(now.ms, abs_timeout.ms);

		_timer.trigger_once(1000*rel_ms);
	}

	void _handle_config()
	{
		_config   .update();
		_blueprint.update();

		if (!_iteration.constructed())
			_iteration.construct(_env, _heap, *this, Iteration::Attr {
				.total_start_time = _start_time, });

		/* propagate update of blueprint */
		_iteration->apply_config_and_blueprint(_config.node(), _blueprint.node());
		_iteration->select_next_test_if_idle();
		_iteration->reconfigure_deploy();
	}

	void _handle_iteration_done()
	{
		Node const &config = _config.node();

		Stats const stats = _iteration->stats(now(), config);

		using Repeat = String<20>;
		Repeat const repeat = config.attribute_value("repeat", Repeat());

		bool const exit = (repeat != "until_forever")
		               && (repeat != "until_failed" || stats.failed);

		if (exit) {
			log("\n--- Finished after ", stats.total_time, " sec ---");

			config.with_optional_sub_node("previous-results", [&] (Node const &node) {
				log("\n", Node::Quoted_content(node)); });

			_iteration.destruct();  /* conclusion printed as side effect */

			log("\n", stats, "\n");

			_env.parent().exit(stats.failed ? -1 : 0);
			return;
		}

		_iteration.destruct();
		_config_handler.local_submit(); /* keep iterating */
	}

	void _handle_timeout()
	{
		if (_iteration.constructed())
			_iteration->handle_timeout_for_current_test(now());
	}

	void _handle_runtime_state()
	{
		_state.update();

		if (!_iteration.constructed() || !_iteration->_plan.any_running())
			return;

		_iteration->_plan.with_running_test([&] (Test &test) {
			_state.node().for_each_sub_node("child", [&] (Node const &child) {
				if (child.attribute_value("name", Test::Name()) == test.name) {
					Exit exit { child.attribute_value("exited", Exit::Code()) };
					if (exit.code.length() > 1)
						_iteration->handle_exit_of_current_test(now(), exit);
				}
			});
		});
	}

	Main(Env &env) : _env(env)
	{
		_config   .sigh(_config_handler);
		_blueprint.sigh(_config_handler);
		_state    .sigh(_runtime_state_handler);
		_timer    .sigh(_timeout_handler);

		_env.parent().announce(_env.ep().manage(_log_root));

		_handle_config();
	}
};


void Component::construct(Genode::Env &env)
{
	static Depot_autopilot::Main main(env);
}

