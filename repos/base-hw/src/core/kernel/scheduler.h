/*
 * \brief   Schedules execution times of a CPU
 * \author  Stefan Kalkowski
 * \author  Johannes Schlatow
 * \date    2014-10-09
 *
 * Implements two-level scheduling scheme with groups of contexts
 * sharing a virtual time, a weight (virtual time factor), and warp
 * value (virtual time shift / latency boost).
 * Within a group each context holds its own virtual time.

 * On the top-level, the group with the lowest virtual time gets
 * selected. Within a group the context with the lowest virtual time
 * gets selected. Whenever the state of the scheduler needs to get
 * updated (timer interrupt, context gets ready/unready, or yield
 * gets called) the virtual time of the current context and group
 * gets increased, depending on the elapsed time and weight of the
 * group.
 *
 * The implementation is strongly related to the scheduler scheme
 * described by Duda and Cheriton in "Borrowed Virtual-Time (BVT)
 * Scheduling" (SOSP99),
 * see https://dl.acm.org/doi/10.1145/319151.319169.
 *
 * We apply the following simplifications/modifications to the BVT scheme:
 *
 * - On the top-level, we have four groups with different weights and warp
 *   values. On the second level, each context has weight 1 and no warp value.
 * - The next timeout is programmed such that the effective virtual time
 *   of the selected context/group $i$ does not exceed any other effective
 *   virtual time by more than $MIN_SCHEDULE_US/weight_i$. This is in contrast
 *   to the definition of context switches from Duda and Cheriton, which uses
 *   actual virtual time instead of effective virtual time.
 * - The (effective) virtual time is considered lowest if it is less or equal
 *   any other virtual time. In consequence, contexts that just got ready (and
 *   have not consumed any CPU time lately) are scheduled immediately.
 * - There is no warp time limit nor unwarp time requirement.
 *
 * For a single-level variant of the above scheduling scheme, we can calculate
 * the following upper bounds on scheduling latency, i.e. the time a context
 * needs to wait until it will get scheduled:
 *
 * - Let $E_i$ and $A_i$ denote the effective and actual virtual time of
 *   a context $i$. Furthermore, let $w_i$ and $e_i$ denote its weight and warp
 *   time. Let $C=MIN_SCHEDULE_US$. Let $SVT$ denote the scheduler virtual time,
 *   $SVT=min_j(A_j)$.
 * - Context $i$ cannot execute ahead of any other context $j$ for more than
 *   $C/w_j$, i.e. $E_i <= E_j + C/w_j$.
 * - Context $i$ cannot execute behind any other context for more than $C/w_j$,
 *   i.e. $E_i >= E_j - C/w_j$.
 * - When context $i$ becomes ready after a longer idle time ($A_i < SVT), its
 *   actual virtual time is set to SVT. Hence, $E_i = SVT - e_i$. All other
 *   contexts $j$ have minimum actual virtual time as well, such that they
 *   execute at most from $E_j=SVT - e_j$ until
 *   $E'_j=E_i + C/w_j = SVT - e_i + C/w_j$. In the worst case, $j$ executes for
 *   for $max(E'_j-E_i, 0)*w_j = max(C+(e_j-e_i)w_j, 0)$ real time before $i$.
 *   The worst-case scheduling latency (in real time) is thus caluclated by
 *   $sum_j{max(C + (e_j-e_i)w_j,0)}$.
 * - When context $i$ becomes ready after it just consumed all its "quota", its
 *   effective virtual time is at most $C/w_i$ ahead of the SVT, hence
 *   $E_i=SVT-e_i + C/w_i$. In the worst case, any other context $j$ would
 *   execute from $E_j=SVT-e_j$ to $E'_j=E_i + C/w_j=SVT - e_i + C/w_j + C/w_i$.
 *   The worst-case scheduling latency (in real time) is thus calculated by
 *   $sum_j{max(E'_j-E_i, 0)*w_j}=sum_j{max(C + C*w_j/w_i + (e_j-e_i)w_j, 0)}$.
 *
 * In consequence, warp times, weights and MIN_SCHEDULE_US can be used for
 * tuning the scheduling latency. While second-level scheduling is only affected
 * by MIN_TIMEOUT_US, the top-level scheduling can be adjusted such that groups
 * with higher warp values experience reduced (or even zero) scheduling
 * interference from groups with lower warp values.
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

				time_t _ready_execution_time { 0 };

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

				auto with_warp(vtime_t warp,
				               time_t  limit,
				               auto const fn) const
				{
					if (_execution_time - _ready_execution_time > limit)
						warp = 0;

					return fn(warp);
				}

				vtime_t vtime(vtime_t warp, time_t limit) const
				{
					return with_warp(warp, limit, [&] (vtime_t const w) {
						return _vtime > w ? _vtime - w : 0; });
				}

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

				/* maximum warped execution time per context */
				time_t const _warp_limit;

				/* group's virtual time */
				vtime_t _vtime { 0 };

				/* minimum virtual time within the group */
				vtime_t _min_vtime { 0 };

				/* ready state on last update() */
				bool    _last_ready { false };

				/* last time the group's ready state changed */
				time_t  _last_state_change { 0 };

				List _contexts {};

				/**
				 * Noncopyable
				 */
				Group(const Group&) = delete;
				Group& operator=(const Group&) = delete;

			public:

				Group(vtime_t weight, vtime_t warp, time_t warp_limit)
				:
					_weight(weight), _warp(warp), _warp_limit(warp_limit) {}

				void insert_orderly(Context &c);
				void remove(Context &c);

				void with_first(auto const fn) const {
					if (_contexts.first()) fn(*_contexts.first()->object()); }

				void add_ticks(time_t ticks) {
					_vtime += (ticks > _weight) ? ticks / _weight : 1; }
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
		/*
		 * Set of scheduling parameters:
		 * - Each group gets a guaranteed CPU share of weight/sum_of_weights. 
		 * - The warp value allows an (idle) group to be preferred over a no-warp
		 *   group for up to weight*warp.
		 * - We account for 10ms execution of the apps group uninterrupted by the
		 *   background group. The apps group gets 5 times more CPU time than the
		 *   background group.
		 * - We account for 10ms execution of the multimedia group uninterrupted
		 *   by the apps group. The multimedia group gets the same share as
		 *   the apps group.
		 * - We account for 5ms execution of the drivers group uninterrupted by
		 *   the multimedia group. The drivers group gets twice as much time as
		 *   the multimedia group. In consequence, the drivers group is granted
		 *   about half of the CPU time. Moreover, it is able to execute for 25ms
		 *   without any interruption by the apps group and 45ms without
		 *   interruptions by the background group.
		 */
		Group _groups[Group_id::MAX + 1] {
			{ 10, _timer.us_to_ticks(4100), _timer.us_to_ticks(50000) }, /* drivers    */
			{  5, _timer.us_to_ticks(4000), _timer.us_to_ticks(50000) }, /* multimedia */
			{  5, _timer.us_to_ticks(2000), _timer.us_to_ticks(50000) }, /* apps       */
			{  1, _timer.us_to_ticks(0),    _timer.us_to_ticks(0) }      /* background */
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

		void _fast_forward(Group &group);

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

		bool current_helping_destination(Context&) const;

		Context& current() const {
			return _current ? *_current : _idle; }
};

#endif /* _CORE__KERNEL__SCHEDULER_H_ */
