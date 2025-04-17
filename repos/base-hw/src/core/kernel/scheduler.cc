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

#include <kernel/scheduler.h>

using namespace Genode;
using namespace Kernel;


void Scheduler::Context::help(Scheduler::Context &c)
{
	_destination = &c;
	c._helper_list.insert(&_helper_le);
}


void Scheduler::Context::helping_finished()
{
	if (!_destination)
		return;

	_destination->_helper_list.remove(&_helper_le);
	_destination = nullptr;
}


Scheduler::Context& Scheduler::Context::helping_destination()
{
	return (_destination) ? _destination->helping_destination() : *this;
}


Scheduler::Context::~Context()
{
	helping_finished();

	for (Context::List_element *h = _helper_list.first(); h; h = h->next())
		h->object()->helping_finished();
}


void Scheduler::Group::insert_orderly(Context &c)
{
	using List_element = Genode::List_element<Context>;

	if (!_contexts.first() ||
	    _contexts.first()->object()->vtime() >= c.vtime()) {
		_contexts.insert(&c._group_le);
		return;
	}

	for (List_element * le = _contexts.first(); le;
	     le = le->next())
		if (!le->next() ||
		    le->next()->object()->vtime() >= c.vtime()) {
			_contexts.insert(&c._group_le, le);
			return;
		}
}


void Scheduler::Group::remove(Context &c)
{
	_contexts.remove(&c._group_le);
}


void Scheduler::Timeout::timeout_triggered()
{
	_scheduler._update_time();
	_scheduler._state = OUT_OF_DATE;
}


bool Scheduler::_earlier(Context const &first, Context const &second) const
{
	if (first.equal_group(second))
		return first._vtime <= second._vtime;

	bool ret = false;
	_with_group(first, [&] (Group const &g1) {
		ret = true;
		_with_group(second, [&] (Group const &g2) {
			ret = g1.earlier(g2); });
	});

	return ret;
}


bool Scheduler::_ready(Group const &group) const
{
	/* ready if there is any context in 'group' */
	if (group._contexts.first())
		return true;

	/* also ready if the current context is the only context in 'group' */
	bool ready = false;
	_with_group(current(), [&] (Group const &cg) {
		if (&cg == &group) ready = true; });
	return ready;
}


void Scheduler::_update_time()
{
	time_t const time = _timer.time();
	time_t const duration = time - _last_time;
	_last_time = time;

	current().helping_destination()._execution_time += duration;

	if (!current().valid())
		return;

	current()._vtime += duration;

	_with_group(current(), [&] (Group &group) {
		group._min_vtime = current()._vtime;
		group.with_first([&] (Context &context) {
			group._min_vtime = min(context._vtime, group._min_vtime); });
		group.add_ticks(duration);
		_min_vtime = group._vtime;
	});

	_for_each_group([&] (Group &group) {
		if (group._contexts.first() && group._vtime < _min_vtime)
			_min_vtime = group._vtime; });
}


void Scheduler::_check_ready_contexts()
{
	if (!_ready_contexts.first())
		return;

	_update_time();

	while (_ready_contexts.first()) {
		Context &c = *_ready_contexts.first()->object();
		_ready_contexts.remove(&c._group_le);

		_with_group(c, [&] (Group &group) {

			/* If group has a vtime in the past, use minimum vtime */
			if (!_ready(group) && (_min_vtime > group._vtime))
				group._vtime = _min_vtime;

			/* if context has a vtime in the past, use groups' minimum time */
			if (group._min_vtime > c._vtime)
				c._vtime = group._min_vtime;

			if (_earlier(c, current()) ||
			    _ticks_distant_to_current(c) < _timer.ticks_left(_timeout))
				_state = OUT_OF_DATE;

			group.insert_orderly(c);
		});
		c._state = Context::READY;
	}
}


time_t Scheduler::_ticks_distant_to_current(Context const &context) const
{
	time_t time = _max_timeout;

	_with_group(current(), [&] (Group const &cur) {
		_with_group(context, [&] (Group const &oth) {
			if (&cur == &oth)
				time = (context._vtime - current()._vtime) + _min_timeout;
			else
				time = ((oth._vtime+cur._warp)
				        - (cur._vtime+oth._warp)) * cur._weight + _min_timeout;
		});
	});

	return time;
}


void Scheduler::update()
{
	using namespace Genode;

	/* move contexts from _ready_contexts into groups */
	_check_ready_contexts();

	if (_up_to_date())
		return;

	/* determine the group with minimum virtual time */
	Context *earliest = &_idle;
	_for_each_group([&] (Group &group) {
		group.with_first([&] (Context &context) {
			if (_earlier(context, *earliest)) earliest = &context; });
	});

	/* switch if earliest group has context earlier than current */
	if (_earlier(*earliest, current())) {
		_with_group(current(), [&] (Group &group) {
			group.insert_orderly(current()); });
		_current = earliest;
		_with_group(current(), [&] (Group &group) {
			group.remove(current()); });
	}

	/* find max run-time till next context should be scheduled */
	time_t ticks_next = _max_timeout;
	_for_each_group([&] (Group const &group) {
		group.with_first([&] (Context &context) {
			time_t t = _ticks_distant_to_current(context);
			if (t < ticks_next) ticks_next = t;
		});
	});
	_timer.set_timeout(_timeout, ticks_next);

	_state = UP_TO_DATE;
}


void Scheduler::ready(Context &c)
{
	if (c.ready())
		return;

	_ready_contexts.insert(&c._group_le);

	c._for_each_helper([this] (Context &helper) { ready(helper); });

	c._state = Context::LISTED;
}


void Scheduler::unready(Context &c)
{
	switch (c._state) {
	case Context::UNREADY:
		return;
	case Context::LISTED:
		_ready_contexts.remove(&c._group_le);
		break;
	case Context::READY:
		_with_group(c, [&] (Group &group) { group.remove(c); });
	};

	c._for_each_helper([this] (Context &helper) { unready(helper); });

	c._state = Context::UNREADY;

	if (!_is_current(c))
		return;

	/* update time before context vanishs */
	_update_time();
	_current = &_idle;
	_state = OUT_OF_DATE;
}


void Scheduler::yield()
{
	/*
	 * When yielding, we want the current context's vtime
	 * reflect the situation as if the context consumed as much of its
	 * time slice so that another context will be scheduled next. As any context's
	 * vtime is never more than _min_timeout behind nor ahead of any other context
	 * (in the same group), adding _min_timeout will basically move another context
	 * to the first position in the group.
	 */
	current()._vtime += _min_timeout;
	_update_time();
	_state = OUT_OF_DATE;
}
