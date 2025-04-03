/*
 * \brief   Schedules CPU shares for the execution time of a CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-10-09
 */

/*
 * Copyright (C) 2014-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__SCHEDULER_H_
#define _CORE__KERNEL__SCHEDULER_H_

/* core includes */
#include <util.h>
#include <util/list.h>
#include <util/misc_math.h>
#include <kernel/configuration.h>
#include <kernel/timer.h>

namespace Kernel { class Scheduler; }


/**
 * Forward declaration of corresponding unit-test classes for friendship
 */
namespace Scheduler_test {
	class Scheduler;
	class Context;
	class Main;
}


class Kernel::Scheduler
{
	public:

		struct Priority
		{
			unsigned value;

			static constexpr unsigned min() { return 0; }
			static constexpr unsigned max() { return cpu_priorities - 1; }

			Priority(unsigned const value)
			: value(Genode::min(value, max())) { }

			Priority &operator =(unsigned const v)
			{
				value = Genode::min(v, max());
				return *this;
			}
		};


		class Context
		{
			private:

				friend class Scheduler;
				friend class Scheduler_test::Scheduler;
				friend class Scheduler_test::Context;

				using List_element = Genode::List_element<Context>;
				using List         = Genode::List<List_element>;

				unsigned     _priority;
				unsigned     _quota;
				List_element _priotized_le { this };
				unsigned     _priotized_time_left { 0 };

				List_element _slack_le { this };
				unsigned     _slack_time_left { 0 };

				List_element _helper_le   { this };
				List         _helper_list {};
				Context     *_destination { nullptr };

				time_t _execution_time { 0 };

				bool _ready { false };

				void _reset() { _priotized_time_left = _quota; }

				/**
				 * Noncopyable
				 */
				Context(const Context&) = delete;
				Context& operator=(const Context&) = delete;

			public:

				Context(Priority const priority,
				        unsigned const quota)
				:
					_priority(priority.value),
					_quota(quota) { }
				~Context();

				bool ready() const { return _ready; }
				void quota(unsigned const q) { _quota = q; }

				void help(Context &c);
				void helping_finished();
				Context& helping_destination();

				time_t execution_time() const {
					return _execution_time; }
		};

	private:

		friend class Scheduler_test::Scheduler;
		friend class Scheduler_test::Main;

		class Context_list
		{
			private:

				using List_element = Genode::List_element<Context>;

				Genode::List<List_element> _list {};
				List_element              *_last { nullptr };

			public:

				void for_each(auto const &fn)
				{
					for (List_element * le = _list.first(); le; le = le->next())
						fn(*le->object());
				}

				void for_each(auto const &fn) const
				{
					for (List_element const * le = _list.first(); le;
					     le = le->next()) fn(*le->object());
				}

				Context* head() const {
					return _list.first() ? _list.first()->object() : nullptr; }

				void insert_head(List_element * const le)
				{
					_list.insert(le);
					if (!_last) _last = le;
				}

				void insert_tail(List_element * const le)
				{
					_list.insert(le, _last);
					_last = le;
				}

				void remove(List_element * const le)
				{
					_list.remove(le);

					if (_last != le)
						return;

					_last = nullptr;
					for (List_element * le = _list.first(); le; le = le->next())
						_last = le;
				}

				void to_tail(List_element * const le)
				{
					remove(le);
					insert_tail(le);
				}

				void to_head(List_element * const le)
				{
					remove(le);
					insert_head(le);
				}
		};

		struct Timeout : Kernel::Timeout
		{
			Scheduler &_scheduler;

			Timeout(Scheduler &scheduler) : _scheduler(scheduler) {}

			virtual void timeout_triggered() override;
		};

		enum State { UP_TO_DATE, OUT_OF_DATE, YIELD };

		unsigned const _slack_quota;
		unsigned const _super_period_length;

		unsigned _super_period_left { _super_period_length };
		unsigned _current_quantum { 0 };

		Timer  &_timer;
		Timeout _timeout { *this };
		time_t  _last_time { 0 };

		State _state { UP_TO_DATE };

		Context_list _rpl[cpu_priorities]; /* ready lists by priority   */
		Context_list _upl[cpu_priorities]; /* unready lists by priority */
		Context_list _slack_list { };

		Context &_idle;
		Context *_current { nullptr };

		void _each_prio_until(auto const &fn)
		{
			for (unsigned p = Priority::max(); p != Priority::min()-1; p--)
				if (fn(p))
					return;
		}

		void _consumed(unsigned const q);
		void _set_current(Context &context, unsigned const q);
		void _account_priotized(Context &, unsigned const r);
		void _account_slack(Context &c, unsigned const r);
		bool _schedule_priotized();
		bool _schedule_slack();

		/**
		 * Noncopyable
		 */
		Scheduler(const Scheduler&) = delete;
		Scheduler& operator=(const Scheduler&) = delete;

	public:

		Scheduler(Timer         &timer,
		          Context       &idle,
		          unsigned const super_period_length,
		          unsigned const slack_quota);

		bool need_to_update() const { return _state != UP_TO_DATE; }

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

		/**
		 * Remove 'context' from scheduler
		 */
		void remove(Context &context);

		/**
		 * Insert 'context' into scheduler
		 */
		void insert(Context &context);

		/**
		 * Set prioritized quota of 'context' to 'quota'
		 */
		void quota(Context &context, unsigned const quota);

		Context& current();
};

#endif /* _CORE__KERNEL__SCHEDULER_H_ */
