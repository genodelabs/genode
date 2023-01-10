/*
 * \brief  Test CPU-scheduler implementation of the kernel
 * \author Martin Stein
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/component.h>

/* core includes */
#include <kernel/cpu_scheduler.h>

using namespace Genode;
using namespace Kernel;

namespace Cpu_scheduler_test {

	class Cpu_share;
	class Cpu_scheduler;
	class Main;

	static Cpu_share &cast(Kernel::Cpu_share &share);

	static Cpu_share const &cast(Kernel::Cpu_share const &share);

	static Double_list<Kernel::Cpu_share> &cast(Double_list<Kernel::Cpu_share> const &list);
}


class Cpu_scheduler_test::Cpu_share : public Kernel::Cpu_share
{
	private:

		using Label = String<32>;

		Label const _label;

	public:

		Cpu_share(Cpu_priority const  prio,
		          unsigned     const  quota,
		          Label        const &label)
		:
			Kernel::Cpu_share { prio, quota },
			_label            { label }
		{ }

		Label const &label() const { return _label; }
};


class Cpu_scheduler_test::Cpu_scheduler : public Kernel::Cpu_scheduler
{
	private:

		template <typename F> void _for_each_prio(F f) const
		{
			bool cancel_for_each_prio { false };
			for (unsigned p = Prio::max(); p != Prio::min() - 1; p--) {
				f(p, cancel_for_each_prio);
				if (cancel_for_each_prio)
					return;
			}
		}

	public:

		void print(Output &output) const;

		using Kernel::Cpu_scheduler::Cpu_scheduler;
};


class Cpu_scheduler_test::Main
{
	private:

		enum { MAX_NR_OF_SHARES = 10 };
		enum { IDLE_SHARE_ID = 0 };
		enum { INVALID_SHARE_ID = MAX_NR_OF_SHARES };

		Env                      &_env;
		Cpu_share                 _idle_share               { 0, 0, (unsigned)IDLE_SHARE_ID };
		Constructible<Cpu_share>  _shares[MAX_NR_OF_SHARES] { };
		time_t                    _current_time             { 0 };
		Cpu_scheduler             _scheduler;

		unsigned _id_of_share(Cpu_share const &share) const
		{
			if (&share == &_idle_share) {

				return IDLE_SHARE_ID;
			}
			unsigned result { INVALID_SHARE_ID };
			for (unsigned id { 1 }; id < sizeof(_shares)/sizeof(_shares[0]);
			     id++) {

				if (_shares[id].constructed() &&
				    (&*_shares[id] == &share)) {

					result = id;
				}
			}
			if (result == INVALID_SHARE_ID) {

				error("failed to determine ID of share");
				_env.parent().exit(-1);
			}
			return result;
		}

		Cpu_share &_share(unsigned const id)
		{
			if (id == IDLE_SHARE_ID) {

				return _idle_share;
			}
			if (id >= sizeof(_shares)/sizeof(_shares[0])) {

				error("failed to determine share by ID");
				_env.parent().exit(-1);
			}
			return *_shares[id];
		}

		void _construct_share(unsigned const id,
		                      unsigned const line_nr)
		{
			if (id == IDLE_SHARE_ID ||
			    id >= sizeof(_shares)/sizeof(_shares[0])) {

				error("failed to construct share in line ", line_nr);
				_env.parent().exit(-1);
			}
			if (_shares[id].constructed()) {

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
			_shares[id].construct(prio, quota, id);
			_scheduler.insert(*_shares[id]);
		}

		void _destroy_share(unsigned const id,
		                    unsigned const line_nr)
		{
			if (id == IDLE_SHARE_ID ||
			    id >= sizeof(_shares)/sizeof(_shares[0])) {

				error("failed to destroy share in line ", line_nr);
				_env.parent().exit(-1);
			}
			if (!_shares[id].constructed()) {

				error("failed to destroy share in line ", line_nr);
				_env.parent().exit(-1);
			}
			_scheduler.remove(_share(id));
			_shares[id].destruct();
		}

		void _update_head_and_check(unsigned const consumed_quota,
		                            unsigned const expected_round_time,
		                            unsigned const expected_head_id,
		                            unsigned const expected_head_quota,
		                            unsigned const line_nr)
		{
			_current_time += consumed_quota;
			_scheduler.update(_current_time);
			unsigned const round_time {
				_scheduler.quota() - _scheduler.residual() };

			if (round_time != expected_round_time) {

				error("wrong time ", round_time, " in line ", line_nr);
				_env.parent().exit(-1);
			}
			Cpu_share &head { cast(_scheduler.head()) };
			unsigned const head_quota { _scheduler.head_quota() };
			if (&head != &_share(expected_head_id)) {

				error("wrong share ", _id_of_share(head), " in line ",
				      line_nr);

				_env.parent().exit(-1);
			}
			if (head_quota != expected_head_quota) {

				error("wrong quota ", head_quota, " in line ", line_nr);
				_env.parent().exit(-1);
			}
		}

		void _set_share_ready_and_check(unsigned const share_id,
		                                bool     const expect_head_outdated,
		                                unsigned const line_nr)
		{
			_scheduler.ready(_share(share_id));
			if (_scheduler.need_to_schedule() != expect_head_outdated) {

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

static Cpu_scheduler_test::Cpu_share &
Cpu_scheduler_test::cast(Kernel::Cpu_share &share)
{
	return *static_cast<Cpu_share *>(&share);
}


static Cpu_scheduler_test::Cpu_share const &
Cpu_scheduler_test::cast(Kernel::Cpu_share const &share)
{
	return *static_cast<Cpu_share const *>(&share);
}


static Double_list<Kernel::Cpu_share> &
Cpu_scheduler_test::cast(Double_list<Kernel::Cpu_share> const &list)
{
	return *const_cast<Double_list<Kernel::Cpu_share> *>(&list);
}


/***************************************
 ** Cpu_scheduler_test::Cpu_scheduler **
 ***************************************/

void Cpu_scheduler_test::Cpu_scheduler::print(Output &output) const
{
	using Genode::print;

	print(output, "(\n");
	print(output, "   quota: ", _residual, "/", _quota, ", ");
	print(output, "fill: ", _fill, ", ");
	print(output, "need to schedule: ", _need_to_schedule ? "true" : "false", ", ");
	print(output, "last_time: ", _last_time);

	if (_head != nullptr) {

		print(output, "\n   head: ( ");
		if (_need_to_schedule) {
			print(output, "\033[31m");
		} else {
			print(output, "\033[32m");
		}
		print(output, "id: ", cast(*_head).label(), ", ");
		print(output, "quota: ", _head_quota, ", ");
		print(output, "\033[0m");
		print(output, "claims: ", _head_claims ? "true" : "false", ", ");
		print(output, "yields: ", _head_yields ? "true" : "false", " ");
		print(output, ")");
	}
	bool prios_empty { true };
	_for_each_prio([&] (Cpu_priority const prio, bool &) {
		if (prios_empty && (_rcl[prio].head() || _ucl[prio].head())) {
			prios_empty = false;
		}
	});
	if (!prios_empty) {

		_for_each_prio([&] (Cpu_priority const prio, bool &) {

			if (_rcl[prio].head() || _ucl[prio].head()) {

				print(output, "\n   prio ", (unsigned)prio, ": (");
				if (_rcl[prio].head()) {

					print(output, " ready: ");
					bool first_share { true };
					cast(_rcl[prio]).for_each([&] (Kernel::Cpu_share const &share) {

						if (&share == _head && _head_claims) {
							if (_need_to_schedule) {
								print(output, "\033[31m");
							} else {
								print(output, "\033[32m");
							}
						}
						print(
							output, first_share ? "" : ", ", cast(share).label() , "-",
							share._claim, "/", share._quota);

						if (&share == _head && _head_claims) {
							print(output, "\033[0m");
						}
						first_share = false;
					});
				}
				if (_ucl[prio].head()) {

					print(output, " unready: ");
					bool first_share { true };
					cast(_ucl[prio]).for_each([&] (Kernel::Cpu_share const &share) {

						print(
							output, first_share ? "" : ", ", cast(share).label() , "-",
							share._claim, "/", share._quota);

						first_share = false;
					});
				}
				print(output, " )");
			}
		});
	}
	if (_fills.head()) {

		print(output, "\n   fills: ");
		bool first_share { true };
		cast(_fills).for_each([&] (Kernel::Cpu_share const &share) {

			if (&share == _head && !_head_claims) {
				if (_need_to_schedule) {
					print(output, "\033[31m");
				} else {
					print(output, "\033[32m");
				}
			}
			print(
				output, first_share ? "" : ", ", cast(share).label() , "-",
				share._fill);

			first_share = false;

			if (&share == _head && !_head_claims) {
				print(output, "\033[0m");
			}
		});
		print(output, "\n");
	}
	print(output, ")");
}


/******************************
 ** Cpu_scheduler_test::Main **
 ******************************/

Cpu_scheduler_test::Main::Main(Env &env)
:
	_env       { env },
	_scheduler { _idle_share, 1000, 100 }
{
	/********************
	 ** Round #1: Idle **
	 ********************/

	_update_head_and_check( 10,  10, 0, 100, __LINE__);
	_update_head_and_check( 90, 100, 0, 100, __LINE__);
	_update_head_and_check(120, 200, 0, 100, __LINE__);
	_update_head_and_check(130, 300, 0, 100, __LINE__);
	_update_head_and_check(140, 400, 0, 100, __LINE__);
	_update_head_and_check(150, 500, 0, 100, __LINE__);
	_update_head_and_check(160, 600, 0, 100, __LINE__);
	_update_head_and_check(170, 700, 0, 100, __LINE__);
	_update_head_and_check(180, 800, 0, 100, __LINE__);
	_update_head_and_check(190, 900, 0, 100, __LINE__);
	_update_head_and_check(200,   0, 0, 100, __LINE__);


	/*************************************
	 ** Round #2: One claim, one filler **
	 *************************************/

	_construct_share(1, __LINE__);
	_update_head_and_check(111, 100, 0, 100, __LINE__);

	_scheduler.ready(_share(1));
	_update_head_and_check(123, 200, 1, 230, __LINE__);

	_scheduler.unready(_share(1));
	_update_head_and_check(200, 400, 0, 100, __LINE__);

	_scheduler.ready(_share(1));
	_update_head_and_check( 10, 410, 1,  30, __LINE__);
	_update_head_and_check(100, 440, 1, 100, __LINE__);
	_update_head_and_check(200, 540, 1, 100, __LINE__);

	_scheduler.unready(_share(1));
	_update_head_and_check(200, 640, 0, 100, __LINE__);
	_update_head_and_check(200, 740, 0, 100, __LINE__);

	_scheduler.ready(_share(1));
	_update_head_and_check( 10, 750, 1, 100, __LINE__);
	_update_head_and_check( 50, 800, 1,  50, __LINE__);
	_update_head_and_check( 20, 820, 1,  30, __LINE__);
	_update_head_and_check(100, 850, 1, 100, __LINE__);
	_update_head_and_check(200, 950, 1,  50, __LINE__);
	_update_head_and_check(200,   0, 1, 230, __LINE__);


	/**************************************
	 ** Round #3: One claim per priority **
	 **************************************/

	_construct_share(2, __LINE__);
	_scheduler.ready(_share(2));
	_update_head_and_check( 50,  50, 1, 180, __LINE__);

	_scheduler.unready(_share(1));
	_update_head_and_check( 70, 120, 2, 170, __LINE__);

	_scheduler.ready(_share(1));
	_scheduler.unready(_share(2));
	_update_head_and_check(110, 230, 1, 110, __LINE__);
	_update_head_and_check( 90, 320, 1,  20, __LINE__);

	_scheduler.ready(_share(2));
	_scheduler.unready(_share(1));
	_update_head_and_check( 10, 330, 2,  60, __LINE__);

	_construct_share(3, __LINE__);
	_update_head_and_check( 40, 370, 2,  20, __LINE__);

	_scheduler.ready(_share(3));
	_update_head_and_check( 10, 380, 3, 110, __LINE__);
	_update_head_and_check(150, 490, 2,  10, __LINE__);
	_update_head_and_check( 10, 500, 2, 100, __LINE__);
	_update_head_and_check( 60, 560, 2,  40, __LINE__);

	_construct_share(4, __LINE__);
	_update_head_and_check( 60, 600, 3, 100, __LINE__);

	_construct_share(6, __LINE__);
	_scheduler.ready(_share(6));
	_update_head_and_check(120, 700, 2, 100, __LINE__);

	_scheduler.ready(_share(4));
	_update_head_and_check( 80, 780, 4,  90, __LINE__);

	_scheduler.unready(_share(4));
	_scheduler.ready(_share(1));
	_update_head_and_check( 50, 830, 1,  10, __LINE__);
	_update_head_and_check( 50, 840, 1, 100, __LINE__);
	_update_head_and_check( 50, 890, 1,  50, __LINE__);
	_update_head_and_check(100, 940, 2,  20, __LINE__);
	_update_head_and_check( 60, 960, 6,  40, __LINE__);
	_update_head_and_check( 60,   0, 3, 110, __LINE__);


	/********************************************
	 ** Round #4: Multiple claims per priority **
	 ********************************************/

	_construct_share(5, __LINE__);
	_update_head_and_check( 60,  60, 3,  50, __LINE__);

	_scheduler.ready(_share(4));
	_scheduler.unready(_share(3));
	_update_head_and_check( 40, 100, 1, 230, __LINE__);

	_construct_share(7, __LINE__);
	_scheduler.ready(_share(7));
	_update_head_and_check(200, 300, 7, 180, __LINE__);

	_construct_share(8, __LINE__);
	_scheduler.ready(_share(5));
	_update_head_and_check(100, 400, 5, 120, __LINE__);

	_scheduler.ready(_share(3));
	_update_head_and_check(100, 500, 3,  10, __LINE__);
	_update_head_and_check( 30, 510, 5,  20, __LINE__);

	_construct_share(9, __LINE__);
	_scheduler.ready(_share(9));
	_update_head_and_check( 10, 520, 5,  10, __LINE__);
	_update_head_and_check( 50, 530, 7,  80, __LINE__);

	_scheduler.ready(_share(8));
	_scheduler.unready(_share(7));
	_update_head_and_check( 10, 540, 8, 100, __LINE__);

	_scheduler.unready(_share(8));
	_update_head_and_check( 80, 620, 1,  30, __LINE__);
	_update_head_and_check(200, 650, 4,  90, __LINE__);
	_update_head_and_check(100, 740, 2, 170, __LINE__);

	_scheduler.ready(_share(8));
	_scheduler.ready(_share(7));
	_update_head_and_check( 10, 750, 7,  70, __LINE__);

	_scheduler.unready(_share(7));
	_scheduler.unready(_share(3));
	_update_head_and_check( 10, 760, 8,  20, __LINE__);

	_scheduler.unready(_share(8));
	_update_head_and_check( 10, 770, 2, 160, __LINE__);

	_scheduler.unready(_share(2));
	_update_head_and_check( 40, 810, 4, 100, __LINE__);

	_scheduler.ready(_share(3));
	_update_head_and_check( 30, 840, 4,  70, __LINE__);
	_update_head_and_check( 80, 910, 1,  90, __LINE__);

	_scheduler.ready(_share(7));
	_scheduler.ready(_share(8));
	_update_head_and_check( 10, 920, 8,  10, __LINE__);
	_update_head_and_check( 30, 930, 7,  60, __LINE__);

	_scheduler.ready(_share(2));
	_scheduler.unready(_share(7));
	_update_head_and_check( 10, 940, 2,  60, __LINE__);

	_scheduler.unready(_share(3));
	_scheduler.unready(_share(5));
	_update_head_and_check( 40, 980, 2,  20, __LINE__);

	_scheduler.unready(_share(9));
	_scheduler.unready(_share(4));
	_update_head_and_check( 10, 990, 2,  10, __LINE__);
	_update_head_and_check( 40,   0, 1, 230, __LINE__);


	/************************************
	 ** Round #5: Yield, ready & check **
	 ************************************/

	_scheduler.unready(_share(6));
	_update_head_and_check( 30,  30, 1, 200, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 20,  50, 8, 100, __LINE__);
	_update_head_and_check(200, 150, 2, 170, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 70, 220, 8, 100, __LINE__);

	_scheduler.unready(_share(8));
	_scheduler.yield();
	_update_head_and_check( 40, 260, 1,  90, __LINE__);

	_scheduler.unready(_share(1));
	_update_head_and_check( 50, 310, 2, 100, __LINE__);
	_update_head_and_check( 10, 320, 2,  90, __LINE__);

	_set_share_ready_and_check(1, false, __LINE__);
	_update_head_and_check(200, 410, 1, 100, __LINE__);
	_update_head_and_check( 10, 420, 1,  90, __LINE__);

	_scheduler.unready(_share(1));
	_update_head_and_check( 10, 430, 2, 100, __LINE__);

	_set_share_ready_and_check(5, true, __LINE__);
	_update_head_and_check( 10, 440, 5, 120, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 90, 530, 2,  90, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 10, 540, 5, 100, __LINE__);

	_set_share_ready_and_check(7, true, __LINE__);
	_update_head_and_check( 10, 550, 7, 180, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 10, 560, 5,  90, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 10, 570, 2, 100, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 10, 580, 7, 100, __LINE__);

	_scheduler.unready(_share(5));
	_update_head_and_check( 10, 590, 7,  90, __LINE__);

	_scheduler.unready(_share(7));
	_set_share_ready_and_check(5, true, __LINE__);
	_update_head_and_check( 10, 600, 2, 100, __LINE__);

	_set_share_ready_and_check(7, false, __LINE__);
	_update_head_and_check(200, 700, 5, 100, __LINE__);

	_scheduler.unready(_share(5));
	_scheduler.unready(_share(7));
	_update_head_and_check( 10, 710, 2, 100, __LINE__);

	_scheduler.unready(_share(2));
	_update_head_and_check( 10, 720, 0, 100, __LINE__);
	_update_head_and_check( 10, 730, 0, 100, __LINE__);
	_update_head_and_check(100, 830, 0, 100, __LINE__);

	_set_share_ready_and_check(9, true, __LINE__);
	_update_head_and_check( 10, 840, 9, 100, __LINE__);

	_set_share_ready_and_check(6, false, __LINE__);
	_update_head_and_check( 20, 860, 9,  80, __LINE__);

	_set_share_ready_and_check(8, false, __LINE__);
	_update_head_and_check( 10, 870, 9,  70, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 10, 880, 6, 100, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 10, 890, 8, 100, __LINE__);

	_set_share_ready_and_check(7, false, __LINE__);
	_scheduler.yield();
	_update_head_and_check( 20, 910, 9,  90, __LINE__);

	_scheduler.unready(_share(8));
	_scheduler.unready(_share(9));
	_update_head_and_check( 10, 920, 6,  80, __LINE__);

	_scheduler.unready(_share(6));
	_scheduler.unready(_share(7));
	_update_head_and_check( 10, 930, 0,  70, __LINE__);

	_set_share_ready_and_check(4, true, __LINE__);
	_update_head_and_check( 20, 950, 4,  50, __LINE__);

	_set_share_ready_and_check(3, true, __LINE__);
	_set_share_ready_and_check(1, true, __LINE__);
	_update_head_and_check( 10, 960, 3,  40, __LINE__);

	_set_share_ready_and_check(5, false, __LINE__);
	_scheduler.unready(_share(4));
	_update_head_and_check( 10, 970, 3,  30, __LINE__);

	_scheduler.unready(_share(3));
	_update_head_and_check( 10, 980, 1,  20, __LINE__);

	_set_share_ready_and_check(3, true, __LINE__);
	_update_head_and_check( 10, 990, 3,  10, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 10,   0, 5, 120, __LINE__);


	/*************************************
	 ** Round #6: Destroy and re-create **
	 *************************************/

	_destroy_share(3, __LINE__);
	_update_head_and_check( 30,  30, 5,  90, __LINE__);

	_scheduler.unready(_share(5));
	_update_head_and_check( 30,  60, 1, 230, __LINE__);

	_destroy_share(4, __LINE__);
	_destroy_share(7, __LINE__);
	_update_head_and_check( 20,  80, 1, 210, __LINE__);

	_scheduler.unready(_share(1));
	_set_share_ready_and_check(9, true, __LINE__);
	_update_head_and_check( 40, 120, 9, 100, __LINE__);

	_scheduler.ready(_share(5));
	_set_share_ready_and_check(8, true, __LINE__);
	_update_head_and_check( 70, 190, 5,  60, __LINE__);

	_destroy_share(8, __LINE__);
	_scheduler.unready(_share(5));
	_update_head_and_check( 10, 200, 9,  30, __LINE__);

	_set_share_ready_and_check(6, false, __LINE__);
	_construct_share(4, __LINE__);
	_update_head_and_check( 10, 210, 9,  20, __LINE__);

	_destroy_share(5, __LINE__);
	_set_share_ready_and_check(4, true, __LINE__);
	_update_head_and_check( 10, 220, 4,  90, __LINE__);
	_update_head_and_check(100, 310, 4, 100, __LINE__);
	_update_head_and_check( 10, 320, 4,  90, __LINE__);

	_destroy_share(4, __LINE__);
	_update_head_and_check(200, 410, 9,  10, __LINE__);

	_construct_share(5, __LINE__);
	_scheduler.ready(_share(5));
	_update_head_and_check( 10, 420, 5, 120, __LINE__);

	_construct_share(4, __LINE__);
	_scheduler.yield();
	_update_head_and_check( 10, 430, 6, 100, __LINE__);

	_set_share_ready_and_check(4, true, __LINE__);
	_scheduler.yield();
	_update_head_and_check( 50, 480, 4,  90, __LINE__);

	_destroy_share(6, __LINE__);
	_scheduler.yield();
	_update_head_and_check( 20, 500, 5, 100, __LINE__);

	_destroy_share(9, __LINE__);
	_update_head_and_check(200, 600, 4, 100, __LINE__);

	_construct_share(7, __LINE__);
	_construct_share(8, __LINE__);
	_update_head_and_check(200, 700, 5, 100, __LINE__);

	_set_share_ready_and_check(1, true, __LINE__);
	_set_share_ready_and_check(7, true, __LINE__);
	_update_head_and_check( 10, 710, 7, 180, __LINE__);

	_set_share_ready_and_check(8, true, __LINE__);
	_update_head_and_check( 40, 750, 8, 100, __LINE__);

	_destroy_share(7, __LINE__);
	_update_head_and_check(200, 850, 1, 150, __LINE__);
	_update_head_and_check( 50, 900, 1, 100, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 60, 960, 8,  40, __LINE__);
	_update_head_and_check(100,   0, 5, 120, __LINE__);


	/**********************************************
	 ** Round #7: Re-configure quota, first part **
	 **********************************************/

	_scheduler.quota(_share(5), 100);
	_construct_share(6, __LINE__);
	_update_head_and_check( 40,  40, 5,  80, __LINE__);

	_scheduler.quota(_share(5), 70);
	_scheduler.ready(_share(6));
	_update_head_and_check( 10,  50, 5,  70, __LINE__);

	_scheduler.quota(_share(5), 40);
	_construct_share(9, __LINE__);
	_update_head_and_check( 10,  60, 5,  40, __LINE__);

	_scheduler.quota(_share(5), 0);
	_scheduler.ready(_share(9));
	_update_head_and_check( 20,  80, 8, 100, __LINE__);

	_scheduler.quota(_share(8), 120);
	_update_head_and_check( 30, 110, 8,  70, __LINE__);

	_scheduler.quota(_share(9), 40);
	_update_head_and_check( 10, 120, 8,  60, __LINE__);

	_scheduler.quota(_share(8), 130);
	_update_head_and_check( 10, 130, 8,  50, __LINE__);

	_scheduler.quota(_share(8), 50);
	_update_head_and_check( 20, 150, 8,  30, __LINE__);

	_scheduler.quota(_share(6), 60);
	_update_head_and_check( 10, 160, 8,  20, __LINE__);

	_scheduler.unready(_share(8));
	_update_head_and_check( 10, 170, 1, 230, __LINE__);

	_scheduler.unready(_share(1));
	_update_head_and_check(100, 270, 4,  90, __LINE__);
	_update_head_and_check(100, 360, 4, 100, __LINE__);

	_scheduler.quota(_share(1), 110);
	_update_head_and_check( 10, 370, 4,  90, __LINE__);

	_scheduler.quota(_share(1), 120);
	_update_head_and_check( 20, 390, 4,  70, __LINE__);

	_scheduler.quota(_share(4), 210);
	_update_head_and_check( 10, 400, 4,  60, __LINE__);

	_scheduler.ready(_share(1));
	_update_head_and_check( 10, 410, 1, 110, __LINE__);

	_scheduler.quota(_share(1), 100);
	_update_head_and_check( 30, 440, 1,  80, __LINE__);

	_set_share_ready_and_check(8, true, __LINE__);
	_update_head_and_check( 10, 450, 8,  10, __LINE__);

	_set_share_ready_and_check(2, false, __LINE__);
	_update_head_and_check( 10, 460, 1,  70, __LINE__);

	_scheduler.quota(_share(1), 20);
	_update_head_and_check( 30, 490, 1,  20, __LINE__);

	_scheduler.quota(_share(1), 50);
	_update_head_and_check( 10, 500, 1,  10, __LINE__);

	_scheduler.quota(_share(1), 0);
	_update_head_and_check( 30, 510, 2, 170, __LINE__);

	_scheduler.quota(_share(2), 180);
	_update_head_and_check( 50, 560, 2, 120, __LINE__);

	_scheduler.unready(_share(2));
	_scheduler.quota(_share(4), 80);
	_update_head_and_check( 70, 630, 8, 100, __LINE__);
	_update_head_and_check( 50, 680, 8,  50, __LINE__);

	_scheduler.unready(_share(8));
	_update_head_and_check( 10, 690, 4,  50, __LINE__);

	_construct_share(3, __LINE__);
	_update_head_and_check( 30, 720, 4,  20, __LINE__);

	_scheduler.quota(_share(3), 210);
	_scheduler.yield();
	_scheduler.unready(_share(4));
	_update_head_and_check( 20, 740, 5,  90, __LINE__);

	_scheduler.unready(_share(9));
	_set_share_ready_and_check(4, false, __LINE__);
	_update_head_and_check( 50, 790, 5,  40, __LINE__);

	_set_share_ready_and_check(2, true, __LINE__);
	_update_head_and_check( 40, 830, 2,  50, __LINE__);
	_update_head_and_check( 60, 880, 2, 100, __LINE__);
	_update_head_and_check( 10, 890, 2,  90, __LINE__);

	_scheduler.yield();
	_set_share_ready_and_check(9, true, __LINE__);
	_update_head_and_check( 60, 950, 6,  50, __LINE__);

	_scheduler.unready(_share(6));
	_update_head_and_check( 20, 970, 1,  30, __LINE__);

	_scheduler.yield();
	_scheduler.ready(_share(8));
	_update_head_and_check( 10, 980, 4,  20, __LINE__);

	_scheduler.unready(_share(8));
	_scheduler.yield();
	_update_head_and_check( 10, 990, 5,  10, __LINE__);

	_scheduler.yield();
	_update_head_and_check( 20,   0, 9,  40, __LINE__);


	/***********************************************
	 ** Round #8: Re-configure quota, second part **
	 ***********************************************/

	_scheduler.ready(_share(3));
	_update_head_and_check( 30,  30, 3, 210, __LINE__);

	_scheduler.unready(_share(3));
	_update_head_and_check(110, 140, 9,  10, __LINE__);
	_update_head_and_check( 40, 150, 4,  80, __LINE__);
	_update_head_and_check(100, 230, 2, 180, __LINE__);

	_scheduler.quota(_share(2), 90);
	_update_head_and_check( 40, 270, 2,  90, __LINE__);

	_scheduler.yield();
	_scheduler.quota(_share(8), 130);
	_update_head_and_check( 40, 310, 4, 100, __LINE__);
	_update_head_and_check(100, 410, 9, 100, __LINE__);

	_scheduler.quota(_share(3), 80);
	_set_share_ready_and_check(3, true, __LINE__);
	_update_head_and_check( 60, 470, 3,  80, __LINE__);

	_set_share_ready_and_check(8, false, __LINE__);
	_scheduler.quota(_share(8), 50);
	_update_head_and_check(100, 550, 8,  50, __LINE__);
	_update_head_and_check( 20, 570, 8,  30, __LINE__);

	_set_share_ready_and_check(6, true, __LINE__);
	_scheduler.quota(_share(6), 50);
	_update_head_and_check( 10, 580, 6,  50, __LINE__);
	_update_head_and_check( 70, 630, 8,  20, __LINE__);
	_update_head_and_check( 40, 650, 8, 100, __LINE__);
	_update_head_and_check( 40, 690, 8,  60, __LINE__);

	_destroy_share(6, __LINE__);
	_update_head_and_check(200, 750, 3, 100, __LINE__);

	_destroy_share(3, __LINE__);
	_update_head_and_check( 90, 840, 9,  40, __LINE__);
	_update_head_and_check( 60, 880, 2, 100, __LINE__);
	_update_head_and_check(120, 980, 1,  20, __LINE__);
	_update_head_and_check( 80,   0, 9,  40, __LINE__);


	_env.parent().exit(0);
}


void Component::construct(Env &env)
{
	static Cpu_scheduler_test::Main main { env };
}
