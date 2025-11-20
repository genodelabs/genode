/*
 * \brief  State of individual test case
 * \author Norman Feske
 * \date   2025-11-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST_H_
#define _TEST_H_

/* local includes */
#include <log_matcher.h>

namespace Depot_autopilot { struct Test; }


struct Depot_autopilot::Test : List_model<Test>::Element
{
	using Name = Child::Name;

	Name const name;

	static Name _name(Node const &n) { return n.attribute_value("name", Name()); }

	bool skip;
	bool malformed = false; /* incomplete/wrong runtime definition */

	using Pkg = String<100>;

	Pkg pkg { };

	enum class State { SCHEDULED, SKIPPED, SELECTED, RUNNING, FAILED, SUCCEEDED };

	State state = State::SCHEDULED;

	Exit exited { };

	Clock start_time { }, end_time { };

	struct Timed_out { unsigned after_seconds; } timed_out { };

	/*
	 * Criterions for detecting the test as failed or succeeded
	 */

	enum   Outcome { FAIL, SUCCEED };
	struct Timeout { unsigned seconds; };
	struct Pattern : Byte_range_ptr { using Byte_range_ptr::Byte_range_ptr; };

	struct Criterion : Interface
	{
		Allocator  &alloc;
		Outcome     outcome;
		Timeout     timeout;
		Exit        exit;
		Pattern     pattern;
		Log_matcher log_matcher { alloc, { pattern.start, pattern.num_bytes } };

		Criterion(Allocator &alloc, Outcome outcome, Timeout timeout, Exit exit,
		          Pattern const &pattern)
		:
			alloc(alloc), outcome(outcome), timeout(timeout), exit(exit),
			pattern(pattern.start, pattern.num_bytes)
		{ }

		~Criterion() { destroy(alloc, pattern.start); }
	};

	Registry<Registered<Criterion>> criterions { };

	bool blueprint_defined = false;

	void add_criterion(Allocator &alloc, Node const &node)
	{
		Outcome const outcome = node.type() == "succeed"
		                      ? Outcome::SUCCEED : Outcome::FAIL;
		Timeout const timeout { node.attribute_value("after_seconds", 0u) };
		Exit    const exit    { node.attribute_value("exit", Exit::Code()) };

		Node::Quoted_content const content { node };

		size_t const n = num_printed_bytes(content);

		Pattern const pattern { (char *)(n ? alloc.alloc(n) : nullptr), n };

		bool const ok = pattern.as_output([&] (Output &out) {
			print(out, content); }).ok();

		if (!ok) warning("failed to decode pattern: ", node);

		new (alloc) Registered<Criterion>(criterions, alloc,
		                                  outcome, timeout, exit, pattern);
	};

	void remove_criterions()
	{
		criterions.for_each([&] (Registered<Criterion> &criterion) {
			destroy(criterion.alloc, &criterion); });
	}

	unsigned max_seconds() const
	{
		if (skip || malformed) return 1;

		unsigned result = 0;
		criterions.for_each([&] (Registered<Criterion> const &criterion) {
			result = max(result, criterion.timeout.seconds); });
		return result;
	}

	Test(Node const &node)
	:
		name(_name(node)), skip(node.attribute_value("skip", false)),
		pkg(node.attribute_value("pkg", Pkg()))
	{ }

	~Test() { remove_criterions(); }

	bool skipped()   const { return state == State::SKIPPED; }
	bool scheduled() const { return state == State::SCHEDULED; }
	bool current()   const { return state == State::SELECTED
	                             || state == State::RUNNING; }
	bool running()   const { return state == State::RUNNING; }
	bool done()      const { return state == State::SKIPPED
	                             || state == State::FAILED
	                             || state == State::SUCCEEDED; }


	static bool type_matches(Node const &node) { return node.type() == "start"; }

	bool matches(Node const &node) const { return _name(node) == name; }

	enum class Deadline_error { AMBIGIOUS, MISSING };
	using Deadline_result = Attempt<Clock, Deadline_error>;

	Deadline_result deadline() const
	{
		unsigned count = 0, seconds = 0;
		criterions.for_each([&] (Registered<Criterion> const &criterion) {
			if (criterion.timeout.seconds) {
				count++;
				seconds = criterion.timeout.seconds; } });

		if (seconds == 0) return Deadline_error::MISSING;
		if (count > 1)    return Deadline_error::AMBIGIOUS;

		return Clock { .ms = start_time.ms + seconds*1000 };
	}

	void _mark_as_finished(Criterion const &criterion, Clock const now)
	{
		end_time = now;

		if (criterion.outcome == Outcome::SUCCEED) state = State::SUCCEEDED;
		if (criterion.outcome == Outcome::FAIL)    state = State::FAILED;
	}

	void _check_criterions_complete(Clock const now)
	{
		criterions.for_each([&] (Registered<Criterion> const &criterion) {
			if (criterion.timeout.seconds)
				return;

			if ((criterion.exit.code == exited.code) && criterion.log_matcher.ok)
				_mark_as_finished(criterion, now);
		});
	}

	void evaluate_timeout(Clock const now)
	{
		deadline().with_result([&] (Clock deadline) {
			if (now.ms < deadline.ms)
				return;

			/* deadline reached, mark test as succeed or failed */
			criterions.for_each([&] (Registered<Criterion> &criterion) {
				if (criterion.timeout.seconds) {
					_mark_as_finished(criterion, now);
					timed_out.after_seconds = criterion.timeout.seconds;
				}});
		}, [&] (Deadline_error) { });
	}

	void evaluate_log(Clock const now, Log_buffer const &log_buffer)
	{
		criterions.for_each([&] (Registered<Criterion> &criterion) {
			criterion.log_matcher.track_and_match(log_buffer); });

		_check_criterions_complete(now);
	}

	void evaluate_exit(Clock const now, Exit::Code const code)
	{
		bool exit_code_expected = false;
		criterions.for_each([&] (Registered<Criterion> const &criterion) {
			if (criterion.exit.code == code)
				exit_code_expected = true; });

		exited.code = code;

		if (exit_code_expected) {
			_check_criterions_complete(now);
			return;
		}

		error(name, " exited with value ", code);
		end_time = now;
		state = State::FAILED;
	}

	void print_conclusion() const
	{
		auto state_name = [&]
		{
			switch (state) {
			case State::FAILED:    return malformed ? "invalid" : "failed";
			case State::SKIPPED:   return "skipped";
			case State::SUCCEEDED: return "ok";
			case State::SCHEDULED:
			case State::SELECTED:
			case State::RUNNING:   break;
			}
			return "?";
		};

		using Reason = String<32>;
		Reason const reason = timed_out.after_seconds
		                    ? Reason { " timeout ", timed_out.after_seconds, " sec" }
		                    : Reason { exited.code.length() > 1 ? " exit" : " log" };

		using Details = String<64>;
		Details details { };

		if (state == State::FAILED || state == State::SUCCEEDED)
			details = { " ", Right_aligned(6, Clock { end_time.ms - start_time.ms }),
			            " ", reason };

		log(" ", Left_aligned(31, name), " ", Left_aligned(9,  state_name()), details);
	}
};

#endif /* _TEST_H_ */
