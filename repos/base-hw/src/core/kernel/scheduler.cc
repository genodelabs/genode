/*
 * \brief   Schedules CPU contexts for the execution time of a CPU
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

#include <types.h>
#include <hw/assert.h>
#include <kernel/scheduler.h>

using namespace Kernel;


void Scheduler::_consumed(unsigned const time)
{
	if (_super_period_left > time) {
		_super_period_left -= time;
		return;
	}

	/**
	 * Start of a new super period
	 */
	_super_period_left = _super_period_length;

	/* reset all priotized contexts */
	_each_prio_until([&] (Priority const priority) {
		unsigned const p = priority.value;
		_rpl[p].for_each([&] (Context &c) { c._reset(); });
		_upl[p].for_each([&] (Context &c) { c._reset(); });
		return false;
	});
}


void Scheduler::_set_current(Context &context, unsigned const q)
{
	_current_quantum = q;
	_current = &context;
}


void Scheduler::_account_priotized(Context &c, unsigned const r)
{
	if (!c._quota)
		return;

	c._priotized_time_left = (r > c._quota) ? c._quota : r;

	if (c._priotized_time_left || !c.ready())
		return;

	_rpl[c._priority].to_tail(&c._priotized_le);

	/*
	 * This is an optimization for the case that a prioritized scheduling
	 * context needs sligtly more time during a round than granted via quota.
	 * If this is the case, we move the scheduling context to the front of
	 * the unprioritized schedule once its quota gets depleted and thereby
	 * at least ensure that it does not have to wait for all unprioritized
	 * scheduling contexts as well before being scheduled again.
	 */
	if (_state != YIELD)
		_slack_list.to_head(&c._slack_le);
}


void Scheduler::_account_slack(Context &c, unsigned const r)
{
	if (r) {
		c._slack_time_left = r;
		return;
	}

	c._slack_time_left = _slack_quota;
	if (c.ready()) _slack_list.to_tail(&c._slack_le);
}


bool Scheduler::_schedule_priotized()
{
	bool result { false };

	_each_prio_until([&] (Priority const p) {
		Context* const context = _rpl[p.value].head();

		if (!context)
			return false;

		if (!context->_priotized_time_left)
			return false;

		_set_current(*context, context->_priotized_time_left);
		result = true;
		return true;
	});
	return result;
}


bool Scheduler::_schedule_slack()
{
	Context *const context = _slack_list.head();
	if (!context)
		return false;

	_set_current(*context, context->_slack_time_left);
	return true;
}


void Scheduler::update(time_t time)
{
	using namespace Genode;

	unsigned const duration =
		min(min((unsigned)(time-_last_time), _current_quantum),
		    _super_period_left);
	_last_time = time;

	/* do not detract the quota of idle or removed context */
	if (_current && _current != &_idle) {
		unsigned const r = (_state != YIELD) ? _current_quantum - duration : 0;
		if (_current->_priotized_time_left) _account_priotized(*_current, r);
		else _account_slack(*_current, r);
	}

	_consumed(duration);

	_state = UP_TO_DATE;

	if (_schedule_priotized())
		return;

	if (_schedule_slack())
		return;

	_set_current(_idle, _slack_quota);
}


void Scheduler::ready(Context &c)
{
	assert(!c.ready() && &c != &_idle);

	c._ready = true;

	bool keep_current =
		(_current->_priotized_time_left &&
		 !c._priotized_time_left) ||
		(_current->_priotized_time_left &&
		 (_current->_priority > c._priority));

	if (c._quota) {
		_upl[c._priority].remove(&c._priotized_le);
		if (c._priotized_time_left)
			_rpl[c._priority].insert_head(&c._priotized_le);
		else
			_rpl[c._priority].insert_tail(&c._priotized_le);
	}

	_slack_list.insert_head(&c._slack_le);

	if (!keep_current && _state == UP_TO_DATE) _state = OUT_OF_DATE;
}


void Scheduler::unready(Context &c)
{
	assert(c.ready() && &c != &_idle);

	if (&c == _current && _state == UP_TO_DATE) _state = OUT_OF_DATE;

	c._ready = false;
	_slack_list.remove(&c._slack_le);

	if (!c._quota)
		return;

	_rpl[c._priority].remove(&c._priotized_le);
	_upl[c._priority].insert_tail(&c._priotized_le);
}


void Scheduler::yield()
{
	_state = YIELD;
}


void Scheduler::remove(Context &c)
{
	assert(&c != &_idle);

	if (c._ready) unready(c);

	if (&c == _current)
		_current = nullptr;

	if (!c._quota)
		return;

	_upl[c._priority].remove(&c._priotized_le);
}


void Scheduler::insert(Context &c)
{
	assert(!c.ready());

	c._slack_time_left = _slack_quota;

	if (!c._quota)
		return;

	c._reset();
	_upl[c._priority].insert_head(&c._priotized_le);
}


void Scheduler::quota(Context &c, unsigned const q)
{
	assert(&c != &_idle);

	if (c._quota) {
		if (c._priotized_time_left > q) c._priotized_time_left = q;

		/* does the quota gets revoked completely? */
		if (!q) {
			if (c.ready()) _rpl[c._priority].remove(&c._priotized_le);
			else           _upl[c._priority].remove(&c._priotized_le);
		}
	} else if (q) {

		/* initial quota introduction */
		if (c.ready()) _rpl[c._priority].insert_tail(&c._priotized_le);
		else           _upl[c._priority].insert_tail(&c._priotized_le);
	}

	c.quota(q);
}


Scheduler::Context& Scheduler::current()
{
	if (!_current) {
		Genode::error("attempt to access invalid scheduler's current context");
		update(_last_time);
	}
	return *_current;
}


Scheduler::Scheduler(Context       &idle,
                     unsigned const super_period_length,
                     unsigned const slack_quota)
:
	_slack_quota(slack_quota),
	_super_period_length(super_period_length),
	_idle(idle)
{
	_set_current(idle, slack_quota);
}
