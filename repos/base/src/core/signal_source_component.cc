/*
 * \brief  Implementation of the SIGNAL interface
 * \author Norman Feske
 * \date   2009-08-11
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/ipc.h>

/* core includes */
#include <signal_source_component.h>

using namespace Genode;


/*****************************
 ** Signal-source component **
 *****************************/

void Signal_source_component::release(Signal_context_component *context)
{
	if (context && context->enqueued())
		_signal_queue.remove(context);
}


void Signal_source_component::submit(Signal_context_component *context,
                                     unsigned long             cnt)
{
	/*
	 * If the client does not block in 'wait_for_signal', the
	 * signal will be delivered as result of the next
	 * 'wait_for_signal' call.
	 */
	context->increment_signal_cnt(cnt);

	/*
	 * If the client is blocking at the signal source (indicated by
	 * the valid reply capability), we wake him up.
	 */
	if (_reply_cap.valid()) {

		_entrypoint->reply_signal_info(_reply_cap, context->imprint(), context->cnt());

		/*
		 * We unblocked the client and, therefore, can invalidate
		 * the reply capability.
		 */
		_reply_cap = Untyped_capability();
		context->reset_signal_cnt();

	} else {

		if (!context->enqueued())
			_signal_queue.enqueue(context);
	}
}


Signal_source::Signal Signal_source_component::wait_for_signal()
{
	/* keep client blocked */
	if (_signal_queue.empty()) {

		/*
		 * Keep reply capability for outstanding request to be used
		 * for the later call of 'explicit_reply()'.
		 */
		_reply_cap = _entrypoint->reply_dst();
		_entrypoint->omit_reply();
		return Signal(0, 0);  /* just a dummy */
	}

	/* dequeue and return pending signal */
	Signal_context_component *context = _signal_queue.dequeue();
	Signal result(context->imprint(), context->cnt());
	context->reset_signal_cnt();
	return result;
}


Signal_source_component::Signal_source_component(Rpc_entrypoint *ep)
:
	_entrypoint(ep)
{ }


Signal_source_component::~Signal_source_component()
{
	if (!_reply_cap.valid())
		return;

	_entrypoint->reply_signal_info(_reply_cap, 0, 0);
	_reply_cap = Untyped_capability();
}
