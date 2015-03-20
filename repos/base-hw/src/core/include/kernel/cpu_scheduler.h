/*
 * \brief   Schedules CPU shares for the execution time of a CPU
 * \author  Martin Stein
 * \date    2014-10-09
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__CPU_SCHEDULER_H_
#define _KERNEL__CPU_SCHEDULER_H_

/* core includes */
#include <util.h>
#include <assert.h>
#include <kernel/configuration.h>
#include <kernel/double_list.h>

namespace Kernel
{
	/**
	 * Priority of an unconsumed CPU claim versus other unconsumed CPU claims
	 */
	class Cpu_priority;

	/**
	 * Scheduling context that has quota and priority (low-latency)
	 */
	class Cpu_claim : public Double_list_item { };

	/**
	 * Scheduling context that has no quota or priority (best effort)
	 */
	class Cpu_fill  : public Double_list_item { };

	/**
	 * Scheduling context that is both claim and fill
	 */
	class Cpu_share;

	/**
	 * Schedules CPU shares for the execution time of a CPU
	 */
	class Cpu_scheduler;
}

class Kernel::Cpu_priority
{
	private:

		unsigned _value;

	public:

		static constexpr signed min = 0;
		static constexpr signed max = cpu_priorities - 1;

		/**
		 * Construct priority with value 'v'
		 */
		Cpu_priority(signed const v) : _value(Genode::min(v, max)) { }

		/*
		 * Standard operators
		 */

		Cpu_priority & operator =(signed const v)
		{
			_value = Genode::min(v, max);
			return *this;
		}

		operator signed() const { return _value; }
};

class Kernel::Cpu_share : public Cpu_claim, public Cpu_fill
{
	friend class Cpu_scheduler;

	private:

		signed const _prio;
		unsigned     _quota;
		unsigned     _claim;
		unsigned     _fill;
		bool         _ready;

	public:

		/**
		 * Constructor
		 *
		 * \param p  claimed priority
		 * \param q  claimed quota
		 */
		Cpu_share(signed const p, unsigned const q)
		: _prio(p), _quota(q), _claim(q), _ready(0) { }

		/*
		 * Accessors
		 */

		bool ready() const { return _ready; }
		void quota(unsigned const q) { _quota = q; }
};

class Kernel::Cpu_scheduler
{
	private:

		typedef Cpu_share                Share;
		typedef Cpu_fill                 Fill;
		typedef Cpu_claim                Claim;
		typedef Double_list_typed<Claim> Claim_list;
		typedef Double_list_typed<Fill>  Fill_list;
		typedef Cpu_priority             Prio;

		Claim_list     _rcl[Prio::max + 1]; /* ready claims */
		Claim_list     _ucl[Prio::max + 1]; /* unready claims */
		Fill_list      _fills;              /* ready fills */
		Share * const  _idle;
		Share *        _head;
		unsigned       _head_quota;
		bool           _head_claims;
		bool           _head_yields;
		unsigned const _quota;
		unsigned       _residual;
		unsigned const _fill;

		template <typename F> void _for_each_prio(F f) {
			for (signed p = Prio::max; p > Prio::min - 1; p--) { f(p); } }

		template <typename T>
		static Share * _share(T * const t) { return static_cast<Share *>(t); }

		static void _reset(Claim * const c) {
			_share(c)->_claim = _share(c)->_quota; }

		void _reset_claims(unsigned const p)
		{
			_rcl[p].for_each([&] (Claim * const c) { _reset(c); });
			_ucl[p].for_each([&] (Claim * const c) { _reset(c); });
		}

		void _next_round()
		{
			_residual = _quota;
			_for_each_prio([&] (unsigned const p) { _reset_claims(p); });
		}

		void _consumed(unsigned const q)
		{
			if (_residual > q) { _residual -= q; }
			else { _next_round(); }
		}

		void _set_head(Share * const s, unsigned const q, bool const c)
		{
			_head_quota = q;
			_head_claims = c;
			_head = s;
		}

		void _next_fill()
		{
			_head->_fill = _fill;
			_fills.head_to_tail();
		}

		void _head_claimed(unsigned const r)
		{
			if (!_head->_quota) { return; }
			_head->_claim = r > _head->_quota ? _head->_quota : r;
			if (_head->_claim || !_head->_ready) { return; }
			_rcl[_head->_prio].to_tail(_head);
		}

		void _head_filled(unsigned const r)
		{
			if (_fills.head() != _head) { return; }
			if (r) { _head->_fill = r; }
			else { _next_fill(); }
		}

		bool _claim_for_head()
		{
			for (signed p = Prio::max; p > Prio::min - 1; p--) {
				Share * const s = _share(_rcl[p].head());
				if (!s) { continue; }
				if (!s->_claim) { continue; }
				_set_head(s, s->_claim, 1);
				return 1;
			}
			return 0;
		}

		bool _fill_for_head()
		{
			Share * const s = _share(_fills.head());
			if (!s) { return 0; }
			_set_head(s, s->_fill, 0);
			return 1;
		}

		unsigned _trim_consumption(unsigned & q)
		{
			q = Genode::min(Genode::min(q, _head_quota), _residual);
			if (!_head_yields) { return _head_quota - q; }
			_head_yields = 0;
			return 0;
		}

		/**
		 * Fill 's' becomes a claim due to a quota donation
		 */
		void _quota_introduction(Share * const s)
		{
			if (s->_ready) { _rcl[s->_prio].insert_tail(s); }
			else { _ucl[s->_prio].insert_tail(s); }
		}

		/**
		 * Claim 's' looses its state as claim due to quota revokation
		 */
		void _quota_revokation(Share * const s)
		{
			if (s->_ready) { _rcl[s->_prio].remove(s); }
			else { _ucl[s->_prio].remove(s); }
		}

		/**
		 * The quota of claim 's' changes to 'q'
		 */
		void _quota_adaption(Share * const s, unsigned const q)
		{
			if (q) { if (s->_claim > q) { s->_claim = q; } }
			else { _quota_revokation(s); }
		}

	public:

		/**
		 * Constructor
		 *
		 * \param i  Gets scheduled with static quota when no other share
		 *           is schedulable. Unremovable. All values get ignored.
		 * \param q  total amount of time quota that can be claimed by shares
		 * \param f  time-slice length of the fill round-robin
		 */
		Cpu_scheduler(Share * const i, unsigned const q, unsigned const f)
		: _idle(i), _head_yields(0), _quota(q), _residual(q), _fill(f)
		{ _set_head(i, f, 0); }

		/**
		 * Update head according to the consumption of quota 'q'
		 */
		void update(unsigned q)
		{
			unsigned const r = _trim_consumption(q);
			if (_head_claims) { _head_claimed(r); }
			else { _head_filled(r); }
			_consumed(q);
			if (_claim_for_head()) { return; }
			if (_fill_for_head()) { return; }
			_set_head(_idle, _fill, 0);
		}

		/**
		 * Set 's1' ready and return wether this outdates current head
		 */
		bool ready_check(Share * const s1)
		{
			ready(s1);
			Share * s2 = _head;
			if (!s1->_claim) { return s2 == _idle; }
			if (!_head_claims) { return 1; }
			if (s1->_prio != s2->_prio) { return s1->_prio > s2->_prio; }
			for (; s2 && s2 != s1; s2 = _share(Claim_list::next(s2))) ;
			return !s2;
		}

		/**
		 * Set share 's' ready
		 */
		void ready(Share * const s)
		{
			assert(!s->_ready && s != _idle);
			s->_ready = 1;
			s->_fill = _fill;
			_fills.insert_tail(s);
			if (!s->_quota) { return; }
			_ucl[s->_prio].remove(s);
			if (s->_claim) { _rcl[s->_prio].insert_head(s); }
			else { _rcl[s->_prio].insert_tail(s); }
		}

		/**
		 * Set share 's' unready
		 */
		void unready(Share * const s)
		{
			assert(s->_ready && s != _idle);
			s->_ready = 0;
			_fills.remove(s);
			if (!s->_quota) { return; }
			_rcl[s->_prio].remove(s);
			_ucl[s->_prio].insert_tail(s);
		}

		/**
		 * Current head looses its current claim/fill for this round
		 */
		void yield() { _head_yields = 1; }

		/**
		 * Remove share 's' from scheduler
		 */
		void remove(Share * const s)
		{
			assert(s != _idle && s != _head);
			if (s->_ready) { _fills.remove(s); }
			if (!s->_quota) { return; }
			if (s->_ready) { _rcl[s->_prio].remove(s); }
			else { _ucl[s->_prio].remove(s); }
		}

		/**
		 * Insert share 's' into scheduler
		 */
		void insert(Share * const s)
		{
			assert(!s->_ready);
			if (!s->_quota) { return; }
			s->_claim = s->_quota;
			_ucl[s->_prio].insert_head(s);
		}

		/**
		 * Set quota of share 's' to 'q'
		 */
		void quota(Share * const s, unsigned const q)
		{
			assert(s != _idle);
			if (s->_quota) { _quota_adaption(s, q); }
			else if (q) { _quota_introduction(s); }
			s->_quota = q;
		}

		/*
		 * Accessors
		 */

		Share * head() const { return _head; }
		unsigned head_quota() const {
			return Genode::min(_head_quota, _residual); }
		unsigned quota() const { return _quota; }
		unsigned residual() const { return _residual; }
};

#endif /* _KERNEL__CPU_SCHEDULER_H_ */
