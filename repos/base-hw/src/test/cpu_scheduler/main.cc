/*
 * \brief  Test CPU-scheduler implementation of the kernel
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/component.h>

/* core includes */
#include <kernel/scheduler.h>

using namespace Genode;
using namespace Kernel;

namespace Scheduler_test {

	class Context;
	class Scheduler;
	class Main;

	using Share_list = List<List_element<Context>>;
	static Context &cast(Kernel::Scheduler::Context &c);

	static Context const &cast(Kernel::Scheduler::Context const &c);
}


class Scheduler_test::Context : public Kernel::Scheduler::Context
{
	private:

		using Priority = Kernel::Scheduler::Priority;
		using Label = String<32>;

		Label const _label;

	public:

		Context(Priority const  prio,
		        unsigned const  quota,
		        Label    const &label)
		:
			Kernel::Scheduler::Context { prio, quota },
			_label { label }
		{ }

		Label const &label() const { return _label; }
};


struct Scheduler_test::Scheduler : Kernel::Scheduler
{
	void _each_prio_until(auto const &fn) const
	{
		for (unsigned p = Priority::max(); p != Priority::min()-1; p--)
			if (fn(p))
				return;
	}

	void print(Output &output) const;

	using Kernel::Scheduler::Scheduler;
};


class Scheduler_test::Main
{
	private:

		enum { MAX_NR_OF_SHARES = 10 };
		enum { IDLE_SHARE_ID = 0 };
		enum { INVALID_SHARE_ID = MAX_NR_OF_SHARES };

		Env                      &_env;
		Context                 _idle_context               { 0, 0, (unsigned)IDLE_SHARE_ID };
		Constructible<Context>  _contexts[MAX_NR_OF_SHARES] { };
		time_t                    _current_time             { 0 };
		Scheduler             _scheduler;

		unsigned _id_of_context(Context const &c) const
		{
			if (&c == &_idle_context) {

				return IDLE_SHARE_ID;
			}
			unsigned result { INVALID_SHARE_ID };
			for (unsigned id { 1 }; id < sizeof(_contexts)/sizeof(_contexts[0]);
			     id++) {

				if (_contexts[id].constructed() &&
				    (&*_contexts[id] == &c)) {

					result = id;
				}
			}
			if (result == INVALID_SHARE_ID) {

				error("failed to determine ID of c");
				_env.parent().exit(-1);
			}
			return result;
		}

		Context &_context(unsigned const id)
		{
			if (id == IDLE_SHARE_ID) {

				return _idle_context;
			}
			if (id >= sizeof(_contexts)/sizeof(_contexts[0])) {

				error("failed to determine share by ID");
				_env.parent().exit(-1);
			}
			return *_contexts[id];
		}

		void _construct_context(unsigned const id,
		                      unsigned const line_nr)
		{
			if (id == IDLE_SHARE_ID ||
			    id >= sizeof(_contexts)/sizeof(_contexts[0])) {

				error("failed to construct share in line ", line_nr);
				_env.parent().exit(-1);
			}
			if (_contexts[id].constructed()) {

				error("failed to construct share in line ", line_nr);
				_env.parent().exit(-1);
			}
			unsigned prio  { 0 };
			unsigned quota { 0 };
			switch (id) {
				case 1: prio = 2; quota = 230; break;
				case 2: prio = 0; quota = 170; break;
				case 3: prio = 3; quota = 110; break;
				case 4: prio = 1; quota =  90; break;
				case 5: prio = 3; quota = 120; break;
				case 6: prio = 3; quota =   0; break;
				case 7: prio = 2; quota = 180; break;
				case 8: prio = 2; quota = 100; break;
				case 9: prio = 2; quota =   0; break;
				default:

					error("failed to construct share in line ", line_nr);
					_env.parent().exit(-1);
					break;
			}
			_contexts[id].construct(prio, quota, id);
			_scheduler.insert(*_contexts[id]);
		}

		void _destroy_context(unsigned const id,
		                    unsigned const line_nr)
		{
			if (id == IDLE_SHARE_ID ||
			    id >= sizeof(_contexts)/sizeof(_contexts[0])) {

				error("failed to destroy share in line ", line_nr);
				_env.parent().exit(-1);
			}
			if (!_contexts[id].constructed()) {

				error("failed to destroy share in line ", line_nr);
				_env.parent().exit(-1);
			}
			_scheduler.remove(_context(id));
			_contexts[id].destruct();
		}

		void _update_current_and_check(unsigned const consumed_time,
		                               unsigned const expected_round_time,
		                               unsigned const expected_current_id,
		                               unsigned const expected_current_time_left,
		                               unsigned const line_nr)
		{
			_current_time += consumed_time;
			_scheduler.update(_current_time);
			unsigned const round_time {
				_scheduler._super_period_length - _scheduler._super_period_left };

			if (round_time != expected_round_time) {

				error("wrong time ", round_time, " in line ", line_nr);
				_env.parent().exit(-1);
			}
			Context &current { cast(_scheduler.current()) };
			unsigned const current_time_left { _scheduler.current_time_left() };
			if (&current != &_context(expected_current_id)) {

				error("wrong share ", _id_of_context(current), " in line ",
				      line_nr);

				_env.parent().exit(-1);
			}
			if (current_time_left != expected_current_time_left) {

				error("wrong quota ", current_time_left, " in line ", line_nr);
				_env.parent().exit(-1);
			}
		}

		void _set_context_ready_and_check(unsigned const share_id,
		                                bool     const expect_current_outdated,
		                                unsigned const line_nr)
		{
			_scheduler.ready(_context(share_id));
			if (_scheduler.need_to_schedule() != expect_current_outdated) {

				error("wrong check result ",
				      _scheduler.need_to_schedule(), " in line ", line_nr);

				_env.parent().exit(-1);
			}
		}

	public:

		Main(Env &env);
};


/*************************
 ** Utilities functions **
 *************************/

static Scheduler_test::Context &
Scheduler_test::cast(Kernel::Scheduler::Context &share)
{
	return *static_cast<Context *>(&share);
}


static Scheduler_test::Context const &
Scheduler_test::cast(Kernel::Scheduler::Context const &share)
{
	return *static_cast<Context const *>(&share);
}


/***************************************
 ** Scheduler_test::Scheduler **
 ***************************************/

void Scheduler_test::Scheduler::print(Output &output) const
{
	using Genode::print;

	print(output, "(\n");
	print(output, "   quota: ", _super_period_left, "/", _super_period_length, ", ");
	print(output, "slack: ", _slack_quota, ", ");
	print(output, "need to schedule: ", need_to_schedule() ? "true" : "false", ", ");
	print(output, "last_time: ", _last_time);

	if (_current != nullptr) {

		print(output, "\n   current: ( ");
		if (need_to_schedule()) {
			print(output, "\033[31m");
		} else {
			print(output, "\033[32m");
		}
		print(output, "id: ", cast(*_current).label(), ", ");
		print(output, "quota: ", _current_quantum, ", ");
		print(output, "\033[0m");
		print(output, "priotized: ", (_current && _current->_priotized_time_left) ? "true" : "false", ", ");
		print(output, "yields: ", _state == YIELD ? "true" : "false", " ");
		print(output, ")");
	}
	bool prios_empty { true };
	_each_prio_until([&] (Priority const prio) {
		if (prios_empty && (_rpl[prio.value].head() || _upl[prio.value].head()))
			prios_empty = false;
		return false;
	});
	if (!prios_empty) {

		_each_prio_until([&] (Priority const prio) {

			unsigned const p = prio.value;
			if (_rpl[p].head() || _upl[p].head()) {

				print(output, "\n   prio ", p, ": (");
				if (_rpl[p].head()) {

					print(output, " ready: ");
					bool first { true };
					_rpl[p].for_each([&] (Kernel::Scheduler::Context const &c) {

						if (&c == _current && _current->_priotized_time_left) {
							if (need_to_schedule()) {
								print(output, "\033[31m");
							} else {
								print(output, "\033[32m");
							}
						}
						print(
							output, first ? "" : ", ", cast(c).label() , "-",
							c._priotized_time_left, "/", c._quota);

						if (&c == _current && _current->_priotized_time_left) {
							print(output, "\033[0m");
						}
						first = false;
					});
				}
				if (_upl[p].head()) {

					print(output, " unready: ");
					bool first { true };
					_upl[p].for_each([&] (Kernel::Scheduler::Context const &c) {

						print(output, first ? "" : ", ", cast(c).label() , "-",
						      c._priotized_time_left, "/", c._quota);

						first = false;
					});
				}
				print(output, " )");
			}
			return false;
		});
	}
	if (_slack_list.head()) {

		print(output, "\n   slacks: ");
		bool first { true };
		_slack_list.for_each([&] (Kernel::Scheduler::Context const &c) {

			if (&c == _current && !_current->_priotized_time_left) {
				if (need_to_schedule()) {
					print(output, "\033[31m");
				} else {
					print(output, "\033[32m");
				}
			}
			print(
				output, first ? "" : ", ", cast(c).label() , "-",
				c._slack_time_left);

			first = false;

			if (&c == _current && !_current->_priotized_time_left) {
				print(output, "\033[0m");
			}
		});
		print(output, "\n");
	}
	print(output, ")");
}


/******************************
 ** Scheduler_test::Main **
 ******************************/

Scheduler_test::Main::Main(Env &env)
:
	_env       { env },
	_scheduler { _idle_context, 1000, 100 }
{
	/********************
	 ** Round #1: Idle **
	 ********************/

	_update_current_and_check( 10,  10, 0, 100, __LINE__);
	_update_current_and_check( 90, 100, 0, 100, __LINE__);
	_update_current_and_check(120, 200, 0, 100, __LINE__);
	_update_current_and_check(130, 300, 0, 100, __LINE__);
	_update_current_and_check(140, 400, 0, 100, __LINE__);
	_update_current_and_check(150, 500, 0, 100, __LINE__);
	_update_current_and_check(160, 600, 0, 100, __LINE__);
	_update_current_and_check(170, 700, 0, 100, __LINE__);
	_update_current_and_check(180, 800, 0, 100, __LINE__);
	_update_current_and_check(190, 900, 0, 100, __LINE__);
	_update_current_and_check(200,   0, 0, 100, __LINE__);


	/******************************************
	 ** Round #2: One prioritized, one slack **
	 *****************************************/

	_construct_context(1, __LINE__);
	_update_current_and_check(111, 100, 0, 100, __LINE__);

	_scheduler.ready(_context(1));
	_update_current_and_check(123, 200, 1, 230, __LINE__);

	_scheduler.unready(_context(1));
	_update_current_and_check(200, 400, 0, 100, __LINE__);

	_scheduler.ready(_context(1));
	_update_current_and_check( 10, 410, 1,  30, __LINE__);
	_update_current_and_check(100, 440, 1, 100, __LINE__);
	_update_current_and_check(200, 540, 1, 100, __LINE__);

	_scheduler.unready(_context(1));
	_update_current_and_check(200, 640, 0, 100, __LINE__);
	_update_current_and_check(200, 740, 0, 100, __LINE__);

	_scheduler.ready(_context(1));
	_update_current_and_check( 10, 750, 1, 100, __LINE__);
	_update_current_and_check( 50, 800, 1,  50, __LINE__);
	_update_current_and_check( 20, 820, 1,  30, __LINE__);
	_update_current_and_check(100, 850, 1, 100, __LINE__);
	_update_current_and_check(200, 950, 1,  50, __LINE__);
	_update_current_and_check(200,   0, 1, 230, __LINE__);


	/******************************************
	 ** Round #3: One priotized per priority **
	 ******************************************/

	_construct_context(2, __LINE__);
	_scheduler.ready(_context(2));
	_update_current_and_check( 50,  50, 1, 180, __LINE__);

	_scheduler.unready(_context(1));
	_update_current_and_check( 70, 120, 2, 170, __LINE__);

	_scheduler.ready(_context(1));
	_scheduler.unready(_context(2));
	_update_current_and_check(110, 230, 1, 110, __LINE__);
	_update_current_and_check( 90, 320, 1,  20, __LINE__);

	_scheduler.ready(_context(2));
	_scheduler.unready(_context(1));
	_update_current_and_check( 10, 330, 2,  60, __LINE__);

	_construct_context(3, __LINE__);
	_update_current_and_check( 40, 370, 2,  20, __LINE__);

	_scheduler.ready(_context(3));
	_update_current_and_check( 10, 380, 3, 110, __LINE__);
	_update_current_and_check(150, 490, 2,  10, __LINE__);
	_update_current_and_check( 10, 500, 2, 100, __LINE__);
	_update_current_and_check( 60, 560, 2,  40, __LINE__);

	_construct_context(4, __LINE__);
	_update_current_and_check( 60, 600, 3, 100, __LINE__);

	_construct_context(6, __LINE__);
	_scheduler.ready(_context(6));
	_update_current_and_check(120, 700, 6, 100, __LINE__);

	_scheduler.ready(_context(4));
	_update_current_and_check( 80, 780, 4,  90, __LINE__);

	_scheduler.unready(_context(4));
	_scheduler.ready(_context(1));
	_update_current_and_check( 50, 830, 1,  10, __LINE__);
	_update_current_and_check( 50, 840, 1,  50, __LINE__);
	_update_current_and_check( 50, 890, 6,  20, __LINE__);
	_update_current_and_check(100, 910, 2,  90, __LINE__);
	_update_current_and_check( 60, 970, 2,  30, __LINE__);
	_update_current_and_check( 60,   0, 3, 110, __LINE__);


	/*************************************************
	 ** Round #4: Multiple prioritized per priority **
	 *************************************************/

	_construct_context(5, __LINE__);
	_update_current_and_check( 60,  60, 3,  50, __LINE__);

	_scheduler.ready(_context(4));
	_scheduler.unready(_context(3));
	_update_current_and_check( 40, 100, 1, 230, __LINE__);

	_construct_context(7, __LINE__);
	_scheduler.ready(_context(7));
	_update_current_and_check(200, 300, 7, 180, __LINE__);

	_construct_context(8, __LINE__);
	_scheduler.ready(_context(5));
	_update_current_and_check(100, 400, 5, 120, __LINE__);

	_scheduler.ready(_context(3));
	_update_current_and_check(100, 500, 3,  10, __LINE__);
	_update_current_and_check( 30, 510, 5,  20, __LINE__);

	_construct_context(9, __LINE__);
	_scheduler.ready(_context(9));
	_update_current_and_check( 10, 520, 5,  10, __LINE__);
	_update_current_and_check( 50, 530, 7,  80, __LINE__);

	_scheduler.ready(_context(8));
	_scheduler.unready(_context(7));
	_update_current_and_check( 10, 540, 8, 100, __LINE__);

	_scheduler.unready(_context(8));
	_update_current_and_check( 80, 620, 1,  30, __LINE__);
	_update_current_and_check(200, 650, 4,  90, __LINE__);
	_update_current_and_check(100, 740, 2, 170, __LINE__);

	_scheduler.ready(_context(8));
	_scheduler.ready(_context(7));
	_update_current_and_check( 10, 750, 7,  70, __LINE__);

	_scheduler.unready(_context(7));
	_scheduler.unready(_context(3));
	_update_current_and_check( 10, 760, 8,  20, __LINE__);

	_scheduler.unready(_context(8));
	_update_current_and_check( 10, 770, 2, 160, __LINE__);

	_scheduler.unready(_context(2));
	_update_current_and_check( 40, 810, 4, 100, __LINE__);

	_scheduler.ready(_context(3));
	_update_current_and_check( 30, 840, 3, 100, __LINE__);
	_update_current_and_check( 70, 910, 3,  30, __LINE__);

	_scheduler.ready(_context(7));
	_scheduler.ready(_context(8));
	_update_current_and_check( 10, 920, 8,  10, __LINE__);
	_update_current_and_check( 30, 930, 7,  60, __LINE__);

	_scheduler.ready(_context(2));
	_scheduler.unready(_context(7));
	_update_current_and_check( 10, 940, 2,  60, __LINE__);

	_scheduler.unready(_context(3));
	_scheduler.unready(_context(5));
	_update_current_and_check( 40, 980, 2,  20, __LINE__);

	_scheduler.unready(_context(9));
	_scheduler.unready(_context(4));
	_update_current_and_check( 10, 990, 2,  10, __LINE__);
	_update_current_and_check( 40,   0, 1, 230, __LINE__);


	/************************************
	 ** Round #5: Yield, ready & check **
	 ************************************/

	_scheduler.unready(_context(6));
	_update_current_and_check( 30,  30, 1, 200, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 20,  50, 8, 100, __LINE__);
	_update_current_and_check(200, 150, 2, 170, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 70, 220, 8, 100, __LINE__);

	_scheduler.unready(_context(8));
	_scheduler.yield();
	_update_current_and_check( 40, 260, 2,  10, __LINE__);
	_update_current_and_check( 40, 270, 1, 100, __LINE__);

	_scheduler.unready(_context(1));
	_update_current_and_check( 40, 310, 2, 100, __LINE__);
	_update_current_and_check( 10, 320, 2,  90, __LINE__);

	_set_context_ready_and_check(1, true, __LINE__);
	_update_current_and_check(200, 410, 1,  60, __LINE__);
	_update_current_and_check( 10, 420, 1,  50, __LINE__);

	_scheduler.unready(_context(1));
	_update_current_and_check( 10, 430, 2, 100, __LINE__);

	_set_context_ready_and_check(5, true, __LINE__);
	_update_current_and_check( 10, 440, 5, 120, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 90, 530, 5,  100, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 10, 540, 2, 90, __LINE__);

	_set_context_ready_and_check(7, true, __LINE__);
	_update_current_and_check( 10, 550, 7, 180, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 10, 560, 7, 100, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 10, 570, 2,  80, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 10, 580, 5, 100, __LINE__);

	_scheduler.unready(_context(5));
	_update_current_and_check( 10, 590, 7, 100, __LINE__);

	_scheduler.unready(_context(7));
	_set_context_ready_and_check(5, true, __LINE__);
	_update_current_and_check( 10, 600, 5,  90, __LINE__);

	_set_context_ready_and_check(7, true, __LINE__);
	_update_current_and_check(200, 690, 7,  90, __LINE__);

	_scheduler.unready(_context(5));
	_scheduler.unready(_context(7));
	_update_current_and_check( 20, 710, 2, 100, __LINE__);

	_scheduler.unready(_context(2));
	_update_current_and_check( 10, 720, 0, 100, __LINE__);
	_update_current_and_check( 10, 730, 0, 100, __LINE__);
	_update_current_and_check(100, 830, 0, 100, __LINE__);

	_set_context_ready_and_check(9, true, __LINE__);
	_update_current_and_check( 10, 840, 9, 100, __LINE__);

	_set_context_ready_and_check(6, true, __LINE__);
	_update_current_and_check( 20, 860, 6, 100, __LINE__);

	_set_context_ready_and_check(8, true, __LINE__);
	_update_current_and_check( 10, 870, 8, 100, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 10, 880, 6,  90, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 10, 890, 9,  80, __LINE__);

	_set_context_ready_and_check(7, true, __LINE__);
	_scheduler.yield();
	_update_current_and_check( 20, 910, 7,  70, __LINE__);

	_scheduler.unready(_context(8));
	_scheduler.unready(_context(9));
	_update_current_and_check( 10, 920, 7,  60, __LINE__);

	_scheduler.unready(_context(6));
	_scheduler.unready(_context(7));
	_update_current_and_check( 10, 930, 0,  70, __LINE__);

	_set_context_ready_and_check(4, true, __LINE__);
	_update_current_and_check( 20, 950, 4,  50, __LINE__);

	_set_context_ready_and_check(3, true, __LINE__);
	_set_context_ready_and_check(1, true, __LINE__);
	_update_current_and_check( 10, 960, 3,  40, __LINE__);

	_set_context_ready_and_check(5, false, __LINE__);
	_scheduler.unready(_context(4));
	_update_current_and_check( 10, 970, 3,  30, __LINE__);

	_scheduler.unready(_context(3));
	_update_current_and_check( 10, 980, 5,  20, __LINE__);

	_set_context_ready_and_check(3, true, __LINE__);
	_update_current_and_check( 10, 990, 3,  10, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 10,   0, 5, 120, __LINE__);


	/*************************************
	 ** Round #6: Destroy and re-create **
	 *************************************/

	_destroy_context(3, __LINE__);
	_update_current_and_check( 30,  30, 5,  90, __LINE__);

	_scheduler.unready(_context(5));
	_update_current_and_check( 30,  60, 1, 230, __LINE__);

	_destroy_context(4, __LINE__);
	_destroy_context(7, __LINE__);
	_update_current_and_check( 20,  80, 1, 210, __LINE__);

	_scheduler.unready(_context(1));
	_set_context_ready_and_check(9, true, __LINE__);
	_update_current_and_check( 40, 120, 9, 100, __LINE__);

	_scheduler.ready(_context(5));
	_set_context_ready_and_check(8, true, __LINE__);
	_update_current_and_check( 70, 190, 5,  60, __LINE__);

	_destroy_context(8, __LINE__);
	_scheduler.unready(_context(5));
	_update_current_and_check( 10, 200, 9,  30, __LINE__);

	_set_context_ready_and_check(6, true, __LINE__);
	_construct_context(4, __LINE__);
	_update_current_and_check( 10, 210, 6, 100, __LINE__);

	_destroy_context(5, __LINE__);
	_set_context_ready_and_check(4, true, __LINE__);
	_update_current_and_check( 10, 220, 4,  90, __LINE__);
	_update_current_and_check(100, 310, 4, 100, __LINE__);
	_update_current_and_check( 10, 320, 4,  90, __LINE__);

	_destroy_context(4, __LINE__);
	_update_current_and_check(200, 410, 6,  90, __LINE__);

	_construct_context(5, __LINE__);
	_scheduler.ready(_context(5));
	_update_current_and_check( 10, 420, 5, 120, __LINE__);

	_construct_context(4, __LINE__);
	_scheduler.yield();
	_update_current_and_check( 10, 430, 5, 100, __LINE__);

	_set_context_ready_and_check(4, true, __LINE__);
	_scheduler.yield();
	_update_current_and_check( 50, 480, 4,  90, __LINE__);

	_destroy_context(6, __LINE__);
	_scheduler.yield();
	_update_current_and_check( 20, 500, 4, 100, __LINE__);

	_destroy_context(9, __LINE__);
	_update_current_and_check(200, 600, 5, 100, __LINE__);

	_construct_context(7, __LINE__);
	_construct_context(8, __LINE__);
	_update_current_and_check(200, 700, 4, 100, __LINE__);

	_set_context_ready_and_check(1, true, __LINE__);
	_set_context_ready_and_check(7, true, __LINE__);
	_update_current_and_check( 10, 710, 7, 180, __LINE__);

	_set_context_ready_and_check(8, true, __LINE__);
	_update_current_and_check( 40, 750, 8, 100, __LINE__);

	_destroy_context(7, __LINE__);
	_update_current_and_check(200, 850, 1, 150, __LINE__);
	_update_current_and_check( 50, 900, 1, 100, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 60, 960, 8,  40, __LINE__);
	_update_current_and_check(100,   0, 5, 120, __LINE__);


	/**********************************************
	 ** Round #7: Re-configure quota, first part **
	 **********************************************/

	_scheduler.quota(_context(5), 100);
	_construct_context(6, __LINE__);
	_update_current_and_check( 40,  40, 5,  80, __LINE__);

	_scheduler.quota(_context(5), 70);
	_scheduler.ready(_context(6));
	_update_current_and_check( 10,  50, 5,  70, __LINE__);

	_scheduler.quota(_context(5), 40);
	_construct_context(9, __LINE__);
	_update_current_and_check( 10,  60, 5,  40, __LINE__);

	_scheduler.quota(_context(5), 0);
	_scheduler.ready(_context(9));
	_update_current_and_check( 20,  80, 8, 100, __LINE__);

	_scheduler.quota(_context(8), 120);
	_update_current_and_check( 30, 110, 8,  70, __LINE__);

	_scheduler.quota(_context(9), 40);
	_update_current_and_check( 10, 120, 8,  60, __LINE__);

	_scheduler.quota(_context(8), 130);
	_update_current_and_check( 10, 130, 8,  50, __LINE__);

	_scheduler.quota(_context(8), 50);
	_update_current_and_check( 20, 150, 8,  30, __LINE__);

	_scheduler.quota(_context(6), 60);
	_update_current_and_check( 10, 160, 8,  20, __LINE__);

	_scheduler.unready(_context(8));
	_update_current_and_check( 10, 170, 1, 230, __LINE__);

	_scheduler.unready(_context(1));
	_update_current_and_check(100, 270, 4,  90, __LINE__);
	_update_current_and_check(100, 360, 4,  90, __LINE__);

	_scheduler.quota(_context(1), 110);
	_update_current_and_check( 10, 370, 4,  80, __LINE__);

	_scheduler.quota(_context(1), 120);
	_update_current_and_check( 20, 390, 4,  60, __LINE__);

	_scheduler.quota(_context(4), 210);
	_update_current_and_check( 10, 400, 4,  50, __LINE__);

	_scheduler.ready(_context(1));
	_update_current_and_check( 10, 410, 1, 110, __LINE__);

	_scheduler.quota(_context(1), 100);
	_update_current_and_check( 30, 440, 1,  80, __LINE__);

	_set_context_ready_and_check(8, true, __LINE__);
	_update_current_and_check( 10, 450, 8,  10, __LINE__);

	_set_context_ready_and_check(2, false, __LINE__);
	_update_current_and_check( 10, 460, 1,  70, __LINE__);

	_scheduler.quota(_context(1), 20);
	_update_current_and_check( 30, 490, 1,  20, __LINE__);

	_scheduler.quota(_context(1), 50);
	_update_current_and_check( 10, 500, 1,  10, __LINE__);

	_scheduler.quota(_context(1), 0);
	_update_current_and_check( 30, 510, 2, 170, __LINE__);

	_scheduler.quota(_context(2), 180);
	_update_current_and_check( 50, 560, 2, 120, __LINE__);

	_scheduler.unready(_context(2));
	_scheduler.quota(_context(4), 80);
	_update_current_and_check( 70, 630, 8,  60, __LINE__);
	_update_current_and_check( 50, 680, 8,  10, __LINE__);

	_scheduler.unready(_context(8));
	_update_current_and_check( 10, 690, 4,  40, __LINE__);

	_construct_context(3, __LINE__);
	_update_current_and_check( 30, 720, 4,  10, __LINE__);

	_scheduler.quota(_context(3), 210);
	_scheduler.yield();
	_scheduler.unready(_context(4));
	_update_current_and_check( 20, 730, 9, 100, __LINE__);

	_scheduler.unready(_context(9));
	_set_context_ready_and_check(4, true, __LINE__);
	_update_current_and_check( 60, 790, 4, 100, __LINE__);

	_set_context_ready_and_check(2, true, __LINE__);
	_update_current_and_check( 40, 830, 2,  50, __LINE__);
	_update_current_and_check( 60, 880, 2,  90, __LINE__);
	_update_current_and_check( 10, 890, 2,  80, __LINE__);

	_scheduler.yield();
	_set_context_ready_and_check(9, true, __LINE__);
	_update_current_and_check( 60, 950, 9,  40, __LINE__);

	_scheduler.unready(_context(6));
	_update_current_and_check( 20, 970, 9,  20, __LINE__);

	_scheduler.yield();
	_scheduler.ready(_context(8));
	_update_current_and_check( 10, 980, 8,  20, __LINE__);

	_scheduler.unready(_context(8));
	_scheduler.yield();
	_update_current_and_check( 10, 990, 4,  10, __LINE__);

	_scheduler.yield();
	_update_current_and_check( 20,   0, 9,  40, __LINE__);


	/***********************************************
	 ** Round #8: Re-configure quota, second part **
	 ***********************************************/

	_scheduler.ready(_context(3));
	_update_current_and_check( 30,  30, 3, 210, __LINE__);

	_scheduler.unready(_context(3));
	_update_current_and_check(110, 140, 9,  10, __LINE__);
	_update_current_and_check( 40, 150, 4,  80, __LINE__);
	_update_current_and_check(100, 230, 2, 180, __LINE__);

	_scheduler.quota(_context(2), 90);
	_update_current_and_check( 40, 270, 2,  90, __LINE__);

	_scheduler.yield();
	_scheduler.quota(_context(8), 130);
	_update_current_and_check( 40, 310, 4, 100, __LINE__);
	_update_current_and_check(100, 410, 9, 100, __LINE__);

	_scheduler.quota(_context(3), 80);
	_set_context_ready_and_check(3, true, __LINE__);
	_update_current_and_check( 60, 470, 3,  80, __LINE__);

	_set_context_ready_and_check(8, false, __LINE__);
	_scheduler.quota(_context(8), 50);
	_update_current_and_check(100, 550, 8,  50, __LINE__);
	_update_current_and_check( 20, 570, 8,  30, __LINE__);

	_set_context_ready_and_check(6, true, __LINE__);
	_scheduler.quota(_context(6), 50);
	_update_current_and_check( 10, 580, 6,  50, __LINE__);
	_update_current_and_check( 70, 630, 8,  20, __LINE__);
	_update_current_and_check( 40, 650, 8, 100, __LINE__);
	_update_current_and_check( 40, 690, 8,  60, __LINE__);

	_destroy_context(6, __LINE__);
	_update_current_and_check(200, 750, 3, 100, __LINE__);

	_destroy_context(3, __LINE__);
	_update_current_and_check( 90, 840, 9,  40, __LINE__);
	_update_current_and_check( 60, 880, 5,  20, __LINE__);
	_update_current_and_check(120, 900, 1, 100, __LINE__);
	_update_current_and_check(100,   0, 9,  40, __LINE__);


	_env.parent().exit(0);
}


void Component::construct(Env &env)
{
	static Scheduler_test::Main main { env };
}
