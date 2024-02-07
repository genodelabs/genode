/*
 * \brief   Schedules CPU shares for the execution time of a CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-10-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__CPU_SCHEDULER_H_
#define _CORE__KERNEL__CPU_SCHEDULER_H_

/* core includes */
#include <util.h>
#include <util/list.h>
#include <util/misc_math.h>
#include <kernel/configuration.h>

namespace Kernel {

	/**
	 * Priority of an unconsumed CPU claim versus other unconsumed CPU claims
	 */
	class Cpu_priority;

	/**
	 * Scheduling context that is both claim and fill
	 */
	class Cpu_share;

	/**
	 * Schedules CPU shares for the execution time of a CPU
	 */
	class Cpu_scheduler;
}


namespace Cpu_scheduler_test {

	/**
	 * Forward declaration of corresponding unit-test classes for friendship
	 */
	class Cpu_scheduler;
	class Cpu_share;
	class Main;
}


class Kernel::Cpu_priority
{
	private:

		unsigned _value;

	public:

		static constexpr unsigned min() { return 0; }
		static constexpr unsigned max() { return cpu_priorities - 1; }

		/**
		 * Construct priority with value 'v'
		 */
		Cpu_priority(unsigned const v)
		:
			_value { Genode::min(v, max()) }
		{ }

		/*
		 * Standard operators
		 */

		Cpu_priority &operator =(unsigned const v)
		{
			_value = Genode::min(v, max());
			return *this;
		}

		operator unsigned() const { return _value; }
};


class Kernel::Cpu_share
{
	friend class Cpu_scheduler;
	friend class Cpu_scheduler_test::Cpu_scheduler;
	friend class Cpu_scheduler_test::Cpu_share;

	private:

		using List_element = Genode::List_element<Cpu_share>;

		List_element       _fill_item  { this };
		List_element       _claim_item { this };
		Cpu_priority const _prio;
		unsigned           _quota;
		unsigned           _claim;
		unsigned           _fill       { 0 };
		bool               _ready      { false };

	public:

		/**
		 * Constructor
		 *
		 * \param p  claimed priority
		 * \param q  claimed quota
		 */
		Cpu_share(Cpu_priority const p, unsigned const q)
		: _prio(p), _quota(q), _claim(q) { }

		/*
		 * Accessors
		 */

		bool ready() const { return _ready; }
		void quota(unsigned const q) { _quota = q; }
};

class Kernel::Cpu_scheduler
{
	friend class Cpu_scheduler_test::Cpu_scheduler;
	friend class Cpu_scheduler_test::Main;

	private:

		class Share_list
		{
			private:

				using List_element = Genode::List_element<Cpu_share>;

				Genode::List<List_element> _list {};
				List_element              *_last { nullptr };

			public:

				template <typename F> void for_each(F const &fn)
				{
					for (List_element * le = _list.first(); le; le = le->next())
						fn(*le->object());
				}

				template <typename F> void for_each(F const &fn) const
				{
					for (List_element const * le = _list.first(); le;
					     le = le->next()) fn(*le->object());
				}

				Cpu_share* head() const {
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

				void head_to_tail() {
					to_tail(_list.first()); }
		};

		typedef Cpu_share    Share;
		typedef Cpu_priority Prio;

		enum State { UP_TO_DATE, OUT_OF_DATE, YIELD };

		State          _state { UP_TO_DATE };
		Share_list     _rcl[Prio::max() + 1]; /* ready claims */
		Share_list     _ucl[Prio::max() + 1]; /* unready claims */
		Share_list     _fills { };          /* ready fills */
		Share         &_idle;
		Share         *_current = nullptr;
		unsigned       _current_quantum { 0 };
		unsigned const _super_period_length;
		unsigned       _super_period_left { _super_period_length };
		unsigned const _fill;
		time_t         _last_time { 0 };

		template <typename F> void _for_each_prio(F f)
		{
			bool cancel_for_each_prio { false };
			for (unsigned p = Prio::max(); p != Prio::min() - 1; p--) {
				f(p, cancel_for_each_prio);
				if (cancel_for_each_prio)
					return;
			}
		}

		static void _reset(Cpu_share &share);

		void _reset_claims(unsigned const p);
		void _consumed(unsigned const q);
		void _set_current(Share &s, unsigned const q);
		void _current_claimed(unsigned const r);
		void _current_filled(unsigned const r);
		bool _schedule_claim();
		bool _schedule_fill();

		unsigned _trim_consumption(unsigned &q);

		/**
		 * Fill 's' becomes a claim due to a quota donation
		 */
		void _quota_introduction(Share &s);

		/**
		 * Claim 's' looses its state as claim due to quota revokation
		 */
		void _quota_revokation(Share &s);

		/**
		 * The quota of claim 's' changes to 'q'
		 */
		void _quota_adaption(Share &s, unsigned const q);

	public:

		/**
		 * Constructor
		 *
		 * \param i  Gets scheduled with static quota when no other share
		 *           is schedulable. Unremovable. All values get ignored.
		 * \param q  total amount of time quota that can be claimed by shares
		 * \param f  time-slice length of the fill round-robin
		 */
		Cpu_scheduler(Share &i, unsigned const q, unsigned const f);

		bool need_to_schedule() const { return _state != UP_TO_DATE; }

		void timeout() {
			if (_state == UP_TO_DATE) _state = OUT_OF_DATE; }

		/**
		 * Update state according to the current (absolute) time
		 */
		void update(time_t time);

		/**
		 * Set share 's' ready
		 */
		void ready(Share &s);

		/**
		 * Set share 's' unready
		 */
		void unready(Share &s);

		/**
		 * Current share likes another share to be scheduled now
		 */
		void yield();

		/**
		 * Remove share 's' from scheduler
		 */
		void remove(Share &s);

		/**
		 * Insert share 's' into scheduler
		 */
		void insert(Share &s);

		/**
		 * Set quota of share 's' to 'q'
		 */
		void quota(Share &s, unsigned const q);

		Share& current();

		unsigned current_time_left() const {
			return Genode::min(_current_quantum, _super_period_left); }
};

#endif /* _CORE__KERNEL__CPU_SCHEDULER_H_ */
