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

#include <base/log.h>
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


void Cpu_scheduler::_next_round()
{
	_residual = _quota;
	_for_each_prio([&] (unsigned const p) { _reset_claims(p); });
}


void Cpu_scheduler::_consumed(unsigned const q)
{
	if (_residual > q) { _residual -= q; }
	else { _next_round(); }
}


void Cpu_scheduler::_set_head(Share &s, unsigned const q, bool const c)
{
	_head_quota = q;
	_head_claims = c;
	_head = &s;
}


void Cpu_scheduler::_next_fill()
{
	_head->_fill = _fill;
	_fills.head_to_tail();
}


void Cpu_scheduler::_head_claimed(unsigned const r)
{
	if (!_head->_quota) { return; }
	_head->_claim = r > _head->_quota ? _head->_quota : r;
	if (_head->_claim || !_head->_ready) { return; }
	_rcl[_head->_prio].to_tail(&_head->_claim_item);
}


void Cpu_scheduler::_head_filled(unsigned const r)
{
	if (_fills.head() != &_head->_fill_item) { return; }
	if (r) { _head->_fill = r; }
	else { _next_fill(); }
}


bool Cpu_scheduler::_claim_for_head()
{
	for (signed p = Prio::MAX; p > Prio::MIN - 1; p--) {
		Double_list_item<Cpu_share> *const item { _rcl[p].head() };
		if (!item) { continue; }
		Cpu_share &share { item->payload() };
		if (!share._claim) { continue; }
		_set_head(share, share._claim, 1);
		return 1;
	}
	return 0;
}


bool Cpu_scheduler::_fill_for_head()
{
	Double_list_item<Cpu_share> *const item { _fills.head() };
	if (!item) {
		return 0;
	}
	Share &share = item->payload();
	_set_head(share, share._fill, 0);
	return 1;
}


unsigned Cpu_scheduler::_trim_consumption(unsigned &q)
{
	q = Genode::min(Genode::min(q, _head_quota), _residual);
	if (!_head_yields) { return _head_quota - q; }
	_head_yields = false;
	return 0;
}


void Cpu_scheduler::_quota_introduction(Share &s)
{
	if (s._ready) { _rcl[s._prio].insert_tail(&s._claim_item); }
	else { _ucl[s._prio].insert_tail(&s._claim_item); }
}


void Cpu_scheduler::_quota_revokation(Share &s)
{
	if (s._ready) { _rcl[s._prio].remove(&s._claim_item); }
	else { _ucl[s._prio].remove(&s._claim_item); }
}


void Cpu_scheduler::_quota_adaption(Share &s, unsigned const q)
{
	if (q) { if (s._claim > q) { s._claim = q; } }
	else { _quota_revokation(s); }
}


void Cpu_scheduler::update(time_t time)
{
	unsigned duration = (unsigned) (time - _last_time);
	_last_time        = time;
	_need_to_schedule = false;

	/* do not detract the quota if the head context was removed even now */
	if (_head) {
		unsigned const r = _trim_consumption(duration);
		if (_head_claims) { _head_claimed(r); }
		else              { _head_filled(r);  }
		_consumed(duration);
	}

	if (_claim_for_head()) { return; }
	if (_fill_for_head()) { return; }
	_set_head(_idle, _fill, 0);
}


void Cpu_scheduler::ready_check(Share &s1)
{
	assert(_head);

	ready(s1);

	if (_need_to_schedule) {
		return;
	}
	Share * s2 = _head;
	if (!s1._claim) {
		_need_to_schedule = s2 == &_idle;
	} else if (!_head_claims) {
		_need_to_schedule = true;
	} else if (s1._prio != s2->_prio) {
		_need_to_schedule = s1._prio > s2->_prio;
	} else {
		for (
			; s2 && s2 != &s1;
			s2 =
				Double_list<Cpu_share>::next(&s2->_claim_item) != nullptr ?
					&Double_list<Cpu_share>::next(&s2->_claim_item)->payload() :
					nullptr) ;

		_need_to_schedule = !s2;
	}
}


void Cpu_scheduler::ready(Share &s)
{
	assert(!s._ready && &s != &_idle);

	_need_to_schedule = true;

	s._ready = 1;
	s._fill = _fill;
	_fills.insert_tail(&s._fill_item);
	if (!s._quota) { return; }
	_ucl[s._prio].remove(&s._claim_item);
	if (s._claim) { _rcl[s._prio].insert_head(&s._claim_item); }
	else { _rcl[s._prio].insert_tail(&s._claim_item); }
}


void Cpu_scheduler::unready(Share &s)
{
	assert(s._ready && &s != &_idle);

	_need_to_schedule = true;

	s._ready = 0;
	_fills.remove(&s._fill_item);
	if (!s._quota) { return; }
	_rcl[s._prio].remove(&s._claim_item);
	_ucl[s._prio].insert_tail(&s._claim_item);
}


void Cpu_scheduler::yield()
{
	_head_yields = true;
	_need_to_schedule = true;
}


void Cpu_scheduler::remove(Share &s)
{
	assert(&s != &_idle);

	_need_to_schedule = true;
	if (&s == _head) _head = nullptr;
	if (s._ready) { _fills.remove(&s._fill_item); }
	if (!s._quota) { return; }
	if (s._ready) { _rcl[s._prio].remove(&s._claim_item); }
	else { _ucl[s._prio].remove(&s._claim_item); }
}


void Cpu_scheduler::insert(Share &s)
{
	assert(!s._ready);
	_need_to_schedule = true;
	if (!s._quota) { return; }
	s._claim = s._quota;
	_ucl[s._prio].insert_head(&s._claim_item);
}


void Cpu_scheduler::quota(Share &s, unsigned const q)
{
	assert(&s != &_idle);
	if (s._quota) { _quota_adaption(s, q); }
	else if (q) { _quota_introduction(s); }
	s._quota = q;
}


Cpu_share &Cpu_scheduler::head() const
{
	assert(_head);
	return *_head;
}


Cpu_scheduler::Cpu_scheduler(Share &i, unsigned const q,
                             unsigned const f)
: _idle(i), _quota(q), _residual(q), _fill(f)
{ _set_head(i, f, 0); }
