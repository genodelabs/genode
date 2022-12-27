/*
 * \brief  Platform driver - shared interrupts
 * \author Stefan Kalkowski
 * \date   2022-09-27
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <shared_irq.h>

using Driver::Shared_interrupt;
using Driver::Shared_interrupt_session;


void Shared_interrupt::_handle()
{
	_sessions.for_each([&] (Shared_interrupt_session & session) {
		session.signal(); });
}


void Shared_interrupt::enable(Irq_session::Trigger  mode,
                              Irq_session::Polarity polarity)
{
	if (!_irq.constructed()) {
		_irq.construct(_env, _number, mode, polarity);
		_irq->sigh(_handler);
	}
}


void Shared_interrupt::disable()
{
	unsigned session_count = 0;
	_sessions.for_each([&] (Shared_interrupt_session &) {
		session_count++; });

	/* if it is the last session, close the upstream connection */
	if (session_count <= 1)
		_irq.destruct();
}


void Shared_interrupt::ack()
{
	unsigned outstanding = 0;
	_sessions.for_each([&] (Shared_interrupt_session & session) {
		if (session.outstanding()) outstanding++; });
	if (!outstanding) _irq->ack_irq();
}


void Shared_interrupt_session::signal()
{
	if (!_cap.valid())
		return;

	_outstanding = true;
	Signal_transmitter(_cap).submit(1);
}
