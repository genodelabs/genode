/*
 * \brief  Test plan
 * \author Norman Feske
 * \date   2025-11-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLAN_H_
#define _PLAN_H_

/* local includes */
#include <test.h>

namespace Depot_autopilot { struct Plan; }


struct Depot_autopilot::Plan
{
	List_model<Test> _tests { };

	struct { Test *_running_ptr = nullptr; };

	void apply_config(Allocator &alloc, Node const &config)
	{
		_tests.update_from_node(config,
			[&] (Node const &node) -> Test & { return *new (alloc) Test(node); },
			[&] (Test &test) { destroy(alloc, &test); },
			[&] (Test &, Node const &) { }
		);
	}

	void apply_blueprint(Allocator &alloc, Node const &blueprint)
	{
		blueprint.for_each_sub_node("pkg", [&] (Node const &pkg) {
			Test::Name const name = pkg.attribute_value("name", Test::Name());
			_tests.for_each([&] (Test &test) {
				if (test.name != name)
					return;

				pkg.with_optional_sub_node("runtime", [&] (Node const &runtime) {

					test.remove_criterions();

					runtime.for_each_sub_node("fail", [&] (Node const &node) {
						test.add_criterion(alloc, node); });

					runtime.for_each_sub_node("succeed", [&] (Node const &node) {
						test.add_criterion(alloc, node); });

					test.blueprint_defined = true;
				});
			});
		});
	}

	void select_next_scheduled()
	{
		_running_ptr = nullptr; /* will be updated in 'with_running_test' */

		bool done = false;
		_tests.for_each([&] (Test &test) {
			if (done)
				return;

			if (test.current())  /* only one test can be current at a time */
				done = true;

			if (test.scheduled()) {
				test.state = Test::State::SELECTED;
				done = true;
			}
		});
	}

	bool running(Test::Name const &name) const
	{
		bool result = false;
		_tests.for_each([&] (Test const &test) {
			if (test.name == name) result = test.running(); });
		return result;
	}

	bool all_done() const
	{
		bool result = true;
		_tests.for_each([&] (Test const &test) {
			if (!test.done()) result = false; });
		return result;
	}

	bool any_running() const
	{
		bool result = false;
		_tests.for_each([&] (Test const &test) {
			if (test.running()) result = true; });
		return result;
	}

	void with_selected_test_not_yet_running(auto const &fn)
	{
		_tests.for_each([&] (Test &test) {
			if (test.state == Test::State::SELECTED)
				fn(test); });
	}

	void with_test(Test::Name const &name, auto const &fn)
	{
		_tests.for_each([&] (Test &test) { if (test.name == name) fn(test); });
	}

	void with_running_test(auto const &fn)
	{
		/*
		 * Frequently called by 'handle_log_message_for_current_test'.
		 * Cache pointer to avoid looping over all tests for each log message.
		 */
		if (!_running_ptr)
			_tests.for_each([&] (Test &test) {
				if (test.running())
					_running_ptr = &test; });

		if (_running_ptr)
			fn(*_running_ptr);
	}

	void with_current_test(auto const &fn)
	{
		_tests.for_each([&] (Test &test) { if (test.current()) fn(test); });
	}

	Stats stats() const
	{
		Stats stats { };
		_tests.for_each([&] (Test const &test) {
			if (test.state == Test::State::SUCCEEDED) stats.succeeded++;
			if (test.state == Test::State::FAILED)    stats.failed++;
			if (test.state == Test::State::SKIPPED)   stats.skipped++;
		});
		return stats;
	}

	void print_conclusions() const
	{
		_tests.for_each([&] (Test const &test) { test.print_conclusion(); });
	}

	void gen_deploy_start_nodes(Generator &g) const
	{
		_tests.for_each([&] (Test const &test) {
			if (!test.skip && !test.malformed)
				g.node("start", [&] {
					g.attribute("name", test.name);
					g.attribute("pkg",  test.pkg); }); });
	}
};

#endif /* _PLAN_H_ */
