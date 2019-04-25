/*
 * \brief   Backend for end points of synchronous interprocess communication
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>

/* core includes */
#include <kernel/ipc_node.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>

using namespace Kernel;


void Ipc_node::_receive_request(Ipc_node &caller)
{
	_thread.ipc_copy_msg(caller._thread);
	_caller = &caller;
	_state  = INACTIVE;
}


void Ipc_node::_receive_reply(Ipc_node &callee)
{
	_thread.ipc_copy_msg(callee._thread);
	_state = INACTIVE;
	_thread.ipc_send_request_succeeded();
}


void Ipc_node::_announce_request(Ipc_node &node)
{
	/* directly receive request if we've awaited it */
	if (_state == AWAIT_REQUEST) {
		_receive_request(node);
		_thread.ipc_await_request_succeeded();
		return;
	}

	/* cannot receive yet, so queue request */
	_request_queue.enqueue(node._request_queue_item);
}


void Ipc_node::_cancel_request_queue()
{
	_request_queue.dequeue_all([] (Queue_item &item) {
		Ipc_node &node { item.object() };
		node._outbuf_request_cancelled();
	});
}


void Ipc_node::_cancel_outbuf_request()
{
	if (_callee) {
		_callee->_announced_request_cancelled(*this);
		_callee = nullptr;
	}
}


void Ipc_node::_cancel_inbuf_request()
{
	if (_caller) {
		_caller->_outbuf_request_cancelled();
		_caller = nullptr;
	}
}


void Ipc_node::_announced_request_cancelled(Ipc_node &node)
{
	if (_caller == &node) _caller = nullptr;
	else _request_queue.remove(node._request_queue_item);
}


void Ipc_node::_outbuf_request_cancelled()
{
	if (_callee == nullptr) return;

	_callee = nullptr;
	_state  = INACTIVE;
	_thread.ipc_send_request_failed();
}


bool Ipc_node::_helps_outbuf_dst() { return (_state == AWAIT_REPLY) && _help; }


bool Ipc_node::can_send_request()
{
	return _state == INACTIVE;
}


void Ipc_node::send_request(Ipc_node &callee, bool help)
{
	_state    = AWAIT_REPLY;
	_callee   = &callee;
	_help     = false;

	/* announce request */
	_callee->_announce_request(*this);

	_help = help;
}


Thread &Ipc_node::helping_sink() {
	return _helps_outbuf_dst() ? _callee->helping_sink() : _thread; }


bool Ipc_node::can_await_request()
{
	return _state == INACTIVE;
}


void Ipc_node::await_request()
{
	_state = AWAIT_REQUEST;
	_request_queue.dequeue([&] (Queue_item &item) {
		_receive_request(item.object());
	});
}


void Ipc_node::send_reply()
{
	/* reply to the last request if we have to */
	if (_state == INACTIVE && _caller) {
		_caller->_receive_reply(*this);
		_caller = nullptr;
	}
}


void Ipc_node::cancel_waiting()
{
	switch (_state) {
	case AWAIT_REPLY:
		_cancel_outbuf_request();
		_state = INACTIVE;
		_thread.ipc_send_request_failed();
		break;
	case AWAIT_REQUEST:
		_state = INACTIVE;
		_thread.ipc_await_request_failed();
		break;
		return;
	default: return;
	}
}


Ipc_node::Ipc_node(Thread &thread)
:
	_thread(thread)
{ }


Ipc_node::~Ipc_node()
{
	_cancel_request_queue();
	_cancel_inbuf_request();
	_cancel_outbuf_request();
}

