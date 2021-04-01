/*
 * \brief  Multiplexing one time source amongst different timeout subjects
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer/timeout.h>
#include <timer_session/connection.h>

using namespace Genode;


/*************
 ** Timeout **
 *************/

void Timeout::schedule_periodic(Microseconds     duration,
                                Timeout_handler &handler)
{
	_scheduler._schedule_periodic_timeout(*this, duration, handler);
}


void Timeout::schedule_one_shot(Microseconds     duration,
                                Timeout_handler &handler)
{
	_scheduler._schedule_one_shot_timeout(*this, duration, handler);
}


Timeout::Timeout(Timeout_scheduler &scheduler)
:
	_scheduler(scheduler)
{ }


Timeout::Timeout(Timer::Connection &timer_connection)
:
	_scheduler(timer_connection._switch_to_timeout_framework_mode())
{ }


Timeout::~Timeout() { _scheduler._destruct_timeout(*this); }

void Timeout::discard() { _scheduler._discard_timeout(*this); }

bool Timeout::scheduled() { return _handler != nullptr; }


/***********************
 ** Timeout_scheduler **
 ***********************/

void Timeout_scheduler::handle_timeout(Duration curr_time)
{
	List<List_element<Timeout> > pending_timeouts { };
	{
		/* acquire scheduler and update stored current time */
		Mutex::Guard const scheduler_guard(_mutex);
		if (_destructor_called) {
			return;
		}
		_current_time = curr_time.trunc_to_plain_us();

		/* apply rate limit to the handling of timeouts */
		if (_current_time.value < _rate_limit_deadline.value) {

			_time_source.set_timeout(
				Microseconds { _rate_limit_deadline.value -
				               _current_time.value },
				*this);

			return;
		}
		_rate_limit_deadline.value = _current_time.value +
		                             _rate_limit_period.value;

		/*
		 * Filter out all pending timeouts to a local list first. The
		 * processing of pending timeouts can have effects on the '_timeouts'
		 * list and these would interfere with the filtering if we would do
		 * it all in the same loop.
		 */
		while (Timeout *timeout = _timeouts.first()) {

			timeout->_mutex.acquire();
			if (timeout->_deadline.value > _current_time.value) {
				timeout->_mutex.release();
				break;
			}
			_timeouts.remove(timeout);
			pending_timeouts.insert(&timeout->_pending_timeouts_le);
		}
		/*
		 * Do the framework-internal processing of the pending timeouts and
		 * then release their mutexes.
		 */
		for (List_element<Timeout> const *elem { pending_timeouts.first() };
		     elem != nullptr;
		     elem = elem->next()) {

			Timeout &timeout { *elem->object() };
			if (!timeout._in_discard_blockade) {

				/*
				 * Remember the handler in an extra member that is altered
				 * only by this code path. This enables us to release the
				 * mutexes of all pending timeouts before starting to call
				 * the timeout handlers. This is necessary to prevent
				 * deadlocks in a situation where multiple timeouts become
				 * pending at once, the handler of the first pending
				 * timeout is about to re-schedule his timeout, and then
				 * a second thread calls 'discard' on another pending
				 * timeout just before that handlers call to 'schedule'.
				 */
				timeout._pending_handler = timeout._handler;

			} else {

				/*
				 * Another thread, that wants to discard the timeout, has been
				 * waiting for a prior call to the timeout handler to finish.
				 * It has already been unblocked again but couldn't continue
				 * discarding the timeout yet. Therefore, we refrain from
				 * calling the timeout handler again until the other thread
				 * could complete its task.
				 */
				pending_timeouts.remove(elem);
			}
			if (timeout._period.value == 0) {

				/* discard one-shot timeouts */
				timeout._handler = nullptr;

			} else {

				/* determine new timeout deadline */
				uint64_t const nr_of_periods {
					((_current_time.value - timeout._deadline.value) /
					 timeout._period.value) + 1 };

				uint64_t deadline_us { timeout._deadline.value +
				                       nr_of_periods * timeout._period.value };

				if (deadline_us < _current_time.value) {
					deadline_us = ~(uint64_t)0;
				}
				/* re-insert timeout into timeouts list */
				timeout._deadline = Microseconds { deadline_us };
				_insert_into_timeouts_list(timeout);
			}
			timeout._mutex.release();
		}
		_set_time_source_timeout();
	}
	/* call the handler of each pending timeout */
	while (List_element<Timeout> const *elem = pending_timeouts.first()) {

		Timeout &timeout { *elem->object() };
		pending_timeouts.remove(elem);

		/*
		 * Timeout handlers are called without holding any timeout mutex or
		 * the scheduler mutex. This ensures that the handler can,
		 * for instance, re-schedule the timeout without running into a
		 * deadlock. The only thing we synchronize is discarding the
		 * timeout. As long as the timeout's '_pending_handler' is set,
		 * a thread that wants to discard the timeout will block at the
		 * timeout's '_discard_blockade'.
		 */
		timeout._pending_handler->handle_timeout(curr_time);

		/*
		 * Unset the timeout's '_pending_handler' again. While the timeout
		 * handler was running, another thread might have tried to discard
		 * the timeout and got blocked at the timeout's '_discard_blockade'.
		 * If this is the case, we have to unblock the other thread.
		 */
		Mutex::Guard timeout_guard(timeout._mutex);
		timeout._pending_handler = nullptr;
		if (timeout._in_discard_blockade) {
			timeout._discard_blockade.wakeup();
		}
	}
}


Timeout_scheduler::Timeout_scheduler(Time_source  &time_source,
                                     Microseconds  rate_limit_period)
:
	_time_source         { time_source },
	_rate_limit_period   { rate_limit_period },
	_rate_limit_deadline { Microseconds { _current_time.value +
	                                      rate_limit_period.value } }
{ }


Timeout_scheduler::~Timeout_scheduler()
{
	/*
	 * Acquire the scheduler mutex and don't release it at the end of this
	 * function to ease debugging in case that someone accesses a dangling
	 * scheduler pointer.
	 */
	_mutex.acquire();

	/*
	 * The function 'Timeout_scheduler::_discard_timeout_unsynchronized' may
	 * have to release and re-acquire the scheduler mutex due to pending
	 * timeout handlers. But, nonetheless, we don't want others to schedule
	 * or discard timeouts while we are emptying the timeout list. Setting
	 * the flag '_destructor_called' causes such attempts to finish without
	 * effect.
	 */
	_destructor_called = true;

	/* discard all scheduled timeouts */
	while (Timeout *timeout = _timeouts.first()) {
		Mutex::Guard const timeout_guard { timeout->_mutex };
		_discard_timeout_unsynchronized(*timeout);
	}
}


void Timeout_scheduler::_enable()
{
	Mutex::Guard const scheduler_guard { _mutex };
	if (_destructor_called) {
		return;
	}
	_set_time_source_timeout();
}


void Timeout_scheduler::_set_time_source_timeout()
{
	_set_time_source_timeout(
		_timeouts.first() ?
			_timeouts.first()->_deadline.value - _current_time.value :
			~(uint64_t)0);
}


void Timeout_scheduler::_set_time_source_timeout(uint64_t duration_us)
{
	if (duration_us < _rate_limit_period.value) {
		duration_us = _rate_limit_period.value;
	}
	if (duration_us > _max_sleep_time.value) {
		duration_us = _max_sleep_time.value;
	}
	_time_source.set_timeout(Microseconds(duration_us), *this);
}


void Timeout_scheduler::_schedule_one_shot_timeout(Timeout         &timeout,
                                                   Microseconds     duration,
                                                   Timeout_handler &handler)
{
	_schedule_timeout(timeout, duration, Microseconds { 0 }, handler);
}


void Timeout_scheduler::_schedule_periodic_timeout(Timeout         &timeout,
                                                   Microseconds     period,
                                                   Timeout_handler &handler)
{

	/* prevent using a period of 0 */
	if (period.value == 0) {
		error("attempt to schedule a periodic timeout of 0");
		return;
	}
	_schedule_timeout(timeout, Microseconds { 0 }, period, handler);
}


void Timeout_scheduler::_schedule_timeout(Timeout         &timeout,
                                          Microseconds     duration,
                                          Microseconds     period,
                                          Timeout_handler &handler)
{
	/* acquire scheduler and timeout mutex */
	Mutex::Guard const scheduler_guard { _mutex };
	if (_destructor_called) {
		return;
	}
	Mutex::Guard const timeout_guard(timeout._mutex);

	/* prevent inserting a timeout twice */
	if (timeout._handler != nullptr) {
		_timeouts.remove(&timeout);
	}
	/* determine timeout deadline */
	uint64_t const curr_time_us {
		_time_source.curr_time().trunc_to_plain_us().value };

	uint64_t const deadline_us {
		duration.value <= ~(uint64_t)0 - curr_time_us ?
			curr_time_us + duration.value : ~(uint64_t)0 };

	/* set up timeout object and insert into timeouts list */
	timeout._handler = &handler;
	timeout._deadline = Microseconds { deadline_us };
	timeout._period = period;
	_insert_into_timeouts_list(timeout);

	/*
	 * If the new timeout is the first to trigger, we have to  update the
	 * time-source timeout.
	 */
	if (_timeouts.first() == &timeout) {
		_set_time_source_timeout(deadline_us - curr_time_us);
	}
}


void Timeout_scheduler::_insert_into_timeouts_list(Timeout &timeout)
{
	/* if timeout list is empty, insert as first element */
	if (_timeouts.first() == nullptr) {
		_timeouts.insert(&timeout);
		return;
	}
	/* if timeout has the shortest deadline, insert as first element */
	if (_timeouts.first()->_deadline.value >= timeout._deadline.value) {
		_timeouts.insert(&timeout);
		return;
	}
	/* find list element with next shorter deadline and insert behind it */
	Timeout *curr_timeout { _timeouts.first() };
	for (;
	     curr_timeout->next() != nullptr &&
	     curr_timeout->next()->_deadline.value < timeout._deadline.value;
	     curr_timeout = curr_timeout->_next);

	_timeouts.insert(&timeout, curr_timeout);
}


void Timeout_scheduler::_discard_timeout(Timeout &timeout)
{
	Mutex::Guard const scheduler_mutex { _mutex };
	Mutex::Guard const timeout_mutex { timeout._mutex };
	_discard_timeout_unsynchronized(timeout);
}


void Timeout_scheduler::_destruct_timeout(Timeout &timeout)
{
	Mutex::Guard const scheduler_mutex { _mutex };

	/*
	 * Acquire the timeout mutex and don't release it at the end of this
	 * function to ease debugging in case that someone accesses a dangling
	 * timeout pointer.
	 */
	timeout._mutex.acquire();
	_discard_timeout_unsynchronized(timeout);
}


void Timeout_scheduler::_discard_timeout_unsynchronized(Timeout &timeout)
{
	if (timeout._pending_handler != nullptr) {

		if (timeout._in_discard_blockade) {
			error("timeout is getting discarded by multiple threads");
		}

		/*
		 * We cannot discard a timeout whose handler is currently executed. We
		 * rather set its flag '_in_discard_blockade' (this ensures that the
		 * timeout handler is not getting called again) and then wait for the
		 * current handler call to finish. 'Timeout_scheduler::handle_timeout'
		 * will wake us up as soon as the handler returned.
		 */
		timeout._in_discard_blockade = true;
		timeout._mutex.release();
		_mutex.release();

		timeout._discard_blockade.block();

		_mutex.acquire();
		timeout._mutex.acquire();
		timeout._in_discard_blockade = false;
	}
	_timeouts.remove(&timeout);
	timeout._handler = nullptr;
}


Duration Timeout_scheduler::curr_time()
{
	Mutex::Guard const scheduler_guard { _mutex };
	if (_destructor_called) {
		return Duration { Microseconds { 0 } };
	}
	return _time_source.curr_time();
}
