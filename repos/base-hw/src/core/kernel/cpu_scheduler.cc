/*
 * \brief   Schedules CPU shares for the execution time of a CPU
 * \author  Martin Stein
 * \date    2014-10-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <types.h>
#include <hw/assert.h>
#include <kernel/cpu_scheduler.h>

using namespace Kernel;


void Cpu_scheduler::_reset(Cpu_share &share)
{
	share._claim = share._quota;
}


void Cpu_scheduler::_reset_claims(unsigned const p)
{
	_rcl[p].for_each([&] (Cpu_share &share) { _reset(share); });
	_ucl[p].for_each([&] (Cpu_share &share) { _reset(share); });
}


void Cpu_scheduler::_consumed(unsigned const q)
{
	if (_super_period_left > q) {
		_super_period_left -= q;
		return;
	}

	_super_period_left = _super_period_length;
	_for_each_prio([&] (Cpu_priority const p, bool &) { _reset_claims(p); });
}


void Cpu_scheduler::_set_current(Share &s, unsigned const q)
{
	_current_quantum = q;
	_current = &s;
}


void Cpu_scheduler::_current_claimed(unsigned const r)
{
	if (!_current->_quota)
		return;

	_current->_claim = r > _current->_quota ? _current->_quota : r;

	if (_current->_claim || !_current->_ready)
		return;

	_rcl[_current->_prio].to_tail(&_current->_claim_item);

	/*
	 * This is an optimization for the case that a prioritized scheduling
	 * context needs sligtly more time during a round than granted via quota.
	 * If this is the case, we move the scheduling context to the front of
	 * the unprioritized schedule once its quota gets depleted and thereby
	 * at least ensure that it does not have to wait for all unprioritized
	 * scheduling contexts as well before being scheduled again.
	 */
	if (_state != YIELD)
		_fills.to_head(&_current->_fill_item);
}


void Cpu_scheduler::_current_filled(unsigned const r)
{
	if (_fills.head() != _current)
		return;

	if (r)
		_current->_fill = r;
	else {
		_current->_fill = _fill;
		_fills.head_to_tail();
	}
}


bool Cpu_scheduler::_schedule_claim()
{
	bool result { false };
	_for_each_prio([&] (Cpu_priority const p, bool &cancel_for_each_prio) {
		Cpu_share* const share = _rcl[p].head();

		if (!share)
			return;

		if (!share->_claim)
			return;

		_set_current(*share, share->_claim);
		result = true;
		cancel_for_each_prio = true;
	});
	return result;
}


bool Cpu_scheduler::_schedule_fill()
{
	Cpu_share *const share = _fills.head();
	if (!share)
		return false;

	_set_current(*share, share->_fill);
	return true;
}


void Cpu_scheduler::_quota_introduction(Share &s)
{
	if (s._ready) _rcl[s._prio].insert_tail(&s._claim_item);
	else          _ucl[s._prio].insert_tail(&s._claim_item);
}


void Cpu_scheduler::_quota_revokation(Share &s)
{
	if (s._ready) _rcl[s._prio].remove(&s._claim_item);
	else          _ucl[s._prio].remove(&s._claim_item);
}


void Cpu_scheduler::_quota_adaption(Share &s, unsigned const q)
{
	if (s._claim > q) s._claim = q;
	if (!q)           _quota_revokation(s);
}


void Cpu_scheduler::update(time_t time)
{
	using namespace Genode;

	unsigned const duration =
		min(min((unsigned)(time-_last_time), _current_quantum),
		    _super_period_left);
	_last_time = time;

	/* do not detract the quota if the current share was removed even now */
	if (_current) {
		unsigned const r = (_state != YIELD) ?  _current_quantum - duration : 0;
		if (_current->_claim) _current_claimed(r);
		else                  _current_filled(r);
	}

	_consumed(duration);

	_state = UP_TO_DATE;

	if (_schedule_claim())
		return;

	if (_schedule_fill())
		return;

	_set_current(_idle, _fill);
}


void Cpu_scheduler::ready(Share &s)
{
	assert(!s._ready && &s != &_idle);

	s._ready = true;

	bool out_of_date = false;

	if (s._quota) {

		_ucl[s._prio].remove(&s._claim_item);
		if (s._claim) {

			_rcl[s._prio].insert_head(&s._claim_item);
			if (_current && _current->_claim) {

				if (s._prio >= _current->_prio) out_of_date = true;
			} else out_of_date = true;
		} else {
			_rcl[s._prio].insert_tail(&s._claim_item);;
		}
	}

	s._fill = _fill;
	_fills.insert_tail(&s._fill_item);

	if (!_current || _current == &_idle) out_of_date = true;

	if (out_of_date && _state == UP_TO_DATE) _state = OUT_OF_DATE;
}


void Cpu_scheduler::unready(Share &s)
{
	assert(s._ready && &s != &_idle);

	if (&s == _current && _state == UP_TO_DATE) _state = OUT_OF_DATE;

	s._ready = false;
	_fills.remove(&s._fill_item);

	if (!s._quota)
		return;

	_rcl[s._prio].remove(&s._claim_item);
	_ucl[s._prio].insert_tail(&s._claim_item);
}


void Cpu_scheduler::yield()
{
	_state = YIELD;
}


void Cpu_scheduler::remove(Share &s)
{
	assert(&s != &_idle);

	if (s._ready) unready(s);

	if (&s == _current)
		_current = nullptr;

	if (!s._quota)
		return;

	_ucl[s._prio].remove(&s._claim_item);
}


void Cpu_scheduler::insert(Share &s)
{
	assert(!s._ready);

	if (!s._quota)
		return;

	s._claim = s._quota;
	_ucl[s._prio].insert_head(&s._claim_item);
}


void Cpu_scheduler::quota(Share &s, unsigned const q)
{
	assert(&s != &_idle);

	if (s._quota)
		_quota_adaption(s, q);
	else if (q)
		_quota_introduction(s);

	s._quota = q;
}


Cpu_share &Cpu_scheduler::current()
{
	if (!_current) {
		Genode::error("attempt to access invalid scheduler's current share");
		update(_last_time);
	}
	return *_current;
}


Cpu_scheduler::Cpu_scheduler(Share         &idle,
                             unsigned const super_period_length,
                             unsigned const f)
:
	_idle(idle),
	_super_period_length(super_period_length),
	_fill(f)
{
	_set_current(idle, f);
}
