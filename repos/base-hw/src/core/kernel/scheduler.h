/*
 * \brief   Schedules execution times of a CPU
 * \author  Stefan Kalkowski
 * \date    2014-10-09
 */

/*
 * Copyright (C) 2014-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__SCHEDULER_H_
#define _CORE__KERNEL__SCHEDULER_H_

/* core includes */
#include <util/list.h>
#include <util/misc_math.h>
#include <kernel/timer.h>
#include <base/log.h>

namespace Kernel { class Scheduler; }


/**
 * Forward declaration of unit-test class
 */
namespace Scheduler_test { class Main; }


class Kernel::Scheduler
{
	public:

		enum { MIN_SCHEDULE_US = 5000 };

		using vtime_t = time_t;

		struct Group_id
		{
			enum Ids : unsigned {
				DRIVER,
				MULTIMEDIA,
				APP,
				BACKGROUND,
				MAX = BACKGROUND,
				INVALID,
			};

			unsigned const value;

			Group_id(unsigned const id) : value(id) { }

			bool valid() const { return value <= MAX; }
		};

		class Context
		{
			private:

				friend class Scheduler;
				friend class Scheduler_test::Main;

				Group_id const _id;

				vtime_t _vtime { 0 };

				time_t _execution_time { 0 };

				enum State { UNREADY, LISTED, READY };

				State _state { UNREADY };

				using List_element = Genode::List_element<Context>;
				using List         = Genode::List<List_element>;

				List_element _group_le { this };

				List_element _helper_le   { this };
				List         _helper_list {};
				Context     *_destination { nullptr };

				void _for_each_helper(auto const fn)
				{
					for (List_element *h = _helper_list.first();
					     h; h = h->next())
						fn(*h->object());
				}

				/**
				 * Noncopyable
				 */
				Context(const Context&) = delete;
				Context& operator=(const Context&) = delete;

			public:

				Context(Group_id const id) : _id(id) {}
				~Context();

				bool ready() const { return _state != UNREADY; }

				bool equal_group(Context const &other) const {
					return _id.value == other._id.value; }

				void help(Context &c);
				void helping_finished();

				Context& helping_destination();

				time_t execution_time() const {
					return _execution_time; }

				vtime_t vtime() const { return _vtime; }

				bool valid() const { return _id.valid(); }
		};

	private:

		friend class Scheduler_test::Main;

		using List = Genode::List<Genode::List_element<Context>>;

		class Group
		{
			private:

				friend class Scheduler;
				friend class Scheduler_test::Main;

				/* higher weight results in slower virtual time */
				vtime_t const _weight;

				/* warp = backwards shift in virtual time */
				vtime_t const _warp;

				/* group's virtual time */
				vtime_t _vtime { 0 };

				/* minimum virtual time within the group */
				vtime_t _min_vtime { 0 };

				List _contexts {};

				/**
				 * Noncopyable
				 */
				Group(const Group&) = delete;
				Group& operator=(const Group&) = delete;

			public:

				Group(vtime_t weight, vtime_t warp)
				:
					_weight(weight), _warp(warp) {}

				void insert_orderly(Context &c);
				void remove(Context &c);

				void with_first(auto const fn) const {
					if (_contexts.first()) fn(*_contexts.first()->object()); }

				void add_ticks(time_t ticks) {
					_vtime += (ticks > _weight) ? ticks / _weight : 1; }

				bool earlier(Group const &other) const {
					return (other._vtime + _warp) >= (_vtime + other._warp); }
		};

		struct Timeout : Kernel::Timeout
		{
			Scheduler &_scheduler;

			Timeout(Scheduler &scheduler) : _scheduler(scheduler) {}

			virtual void timeout_triggered() override;
		};

		Timer  &_timer;
		Timeout _timeout { *this };
		time_t  _min_timeout { _timer.us_to_ticks(MIN_SCHEDULE_US) };
		time_t  _max_timeout { _timer.us_to_ticks(_timer.timeout_max_us()) };
		time_t  _last_time { 0 };

		/* minimum virtual time of all groups */
		vtime_t _min_vtime { 0 };

		enum State { UP_TO_DATE, OUT_OF_DATE }
			_state { UP_TO_DATE };

		Context &_idle;
		Context *_current { &_idle };

		/* stores LISTED contexts, will be moved into groups by update() */
		List _ready_contexts {};

		/* The guaranteed CPU share of a group calculates as weight/sum_of_weights */
		Group _groups[Group_id::MAX + 1] {
			{ 35, _timer.us_to_ticks(800) }, /* drivers    */
			{ 10, _timer.us_to_ticks(400) }, /* multimedia */
			{  4, _timer.us_to_ticks(200) }, /* apps       */
			{  1, _timer.us_to_ticks(  0) }  /* background */
		};

		void _for_each_group(auto const fn) {
			for (unsigned i = 0; i <= Group_id::MAX; i++) fn(_groups[i]); }

		void _update_time();

		bool _is_current(Context &c) const {
			return _current == &c; }

		bool _up_to_date() const {
			return _state == UP_TO_DATE; }

		void _with_group(Context const &c, auto const fn) {
			if (c._id.valid()) fn(_groups[c._id.value]); }

		void _with_group(Context const &c, auto const fn) const {
			if (c._id.valid()) fn(_groups[c._id.value]); }

		bool _earlier(Context const &first, Context const &second) const;

		bool _ready(Group const &group) const;

		void _check_ready_contexts();

		time_t _ticks_distant_to_current(Context const &context) const;

		/**
		 * Noncopyable
		 */
		Scheduler(const Scheduler&) = delete;
		Scheduler& operator=(const Scheduler&) = delete;

	public:

		Scheduler(Timer &timer, Context &idle)
		:
			_timer(timer), _idle(idle) { }

		/**
		 * Update state
		 */
		void update();

		/**
		 * Set 'context' ready
		 */
		void ready(Context &context);

		/**
		 * Set 'context' unready
		 */
		void unready(Context &context);

		/**
		 * Current context likes another context to be scheduled now
		 */
		void yield();

		Context& current() const {
			return _current ? *_current : _idle; }
};

#endif /* _CORE__KERNEL__SCHEDULER_H_ */
