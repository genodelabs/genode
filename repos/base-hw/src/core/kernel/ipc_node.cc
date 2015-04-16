/*
 * \brief   Backend for end points of synchronous interprocess communication
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/string.h>

/* core includes */
#include <kernel/ipc_node.h>
#include <assert.h>

using namespace Kernel;


void Ipc_node::_receive_request(Message_buf * const r)
{
	/* FIXME: invalid requests should be discarded */
	if (r->size > _inbuf.size) {
		PWRN("oversized request");
		r->size = _inbuf.size;
	}
	/* fetch message */
	Genode::memcpy(_inbuf.base, r->base, r->size);
	_inbuf.size = r->size;
	_inbuf.src  = r->src;

	/* update state */
	_state = PREPARE_REPLY;
}


void Ipc_node::_receive_reply(void * const base, size_t const size)
{
	/* FIXME: when discard awaited replies userland must get a hint */
	if (size > _inbuf.size) {
		PDBG("discard invalid IPC reply");
		return;
	}
	/* receive reply */
	Genode::memcpy(_inbuf.base, base, size);
	_inbuf.size = size;

	/* update state */
	if (_state != PREPARE_AND_AWAIT_REPLY) { _state = INACTIVE; }
	else { _state = PREPARE_REPLY; }
	_send_request_succeeded();
}


void Ipc_node::_announce_request(Message_buf * const r)
{
	/* directly receive request if we've awaited it */
	if (_state == AWAIT_REQUEST) {
		_receive_request(r);
		_await_request_succeeded();
		return;
	}
	/* cannot receive yet, so queue request */
	_request_queue.enqueue(r);
}


void Ipc_node::_cancel_request_queue()
{
	while (1) {
		Message_buf * const r = _request_queue.dequeue();
		if (!r) { return; }
		r->src->_outbuf_request_cancelled();
	}
}


void Ipc_node::_cancel_outbuf_request()
{
	if (_outbuf_dst) {
		_outbuf_dst->_announced_request_cancelled(&_outbuf);
		_outbuf_dst = 0;
	}
}


void Ipc_node::_cancel_inbuf_request()
{
	if (_inbuf.src) {
		_inbuf.src->_outbuf_request_cancelled();
		_inbuf.src = 0;
	}
}


void Ipc_node::_announced_request_cancelled(Message_buf * const r)
{
	if (_inbuf.src == r->src) {
		_inbuf.src = 0;
		return;
	}
	_request_queue.remove(r);
}


void Ipc_node::_outbuf_request_cancelled()
{
	if (_outbuf_dst) {
		_outbuf_dst = 0;
		if (!_inbuf.src) { _state = INACTIVE; }
		else { _state = PREPARE_REPLY; }
		_send_request_failed();
	}
}


bool Ipc_node::_helps_outbuf_dst()
{
	return (_state == PREPARE_AND_AWAIT_REPLY ||
	        _state == AWAIT_REPLY) && _outbuf_dst_help;
}


void Ipc_node::send_request(Ipc_node * const dst, void * const buf_base,
                            size_t const buf_size, size_t const msg_size,
                            bool help)
{
	/* assertions */
	assert(_state == INACTIVE || _state == PREPARE_REPLY);

	/* prepare transmission of request message */
	_outbuf.base     = buf_base;
	_outbuf.size     = msg_size;
	_outbuf.src      = this;
	_outbuf_dst      = dst;
	_outbuf_dst_help = 0;

	/*
	 * Prepare reception of reply message but don't clear
	 * '_inbuf.origin' because we might also prepare a reply.
	 */
	_inbuf.base = buf_base;
	_inbuf.size = buf_size;

	/* update state */
	if (_state != PREPARE_REPLY) { _state = AWAIT_REPLY; }
	else { _state = PREPARE_AND_AWAIT_REPLY; }

	/* announce request */
	dst->_announce_request(&_outbuf);

	/* set help relation after announcement to simplify scheduling */
	_outbuf_dst_help = help;
}


Ipc_node * Ipc_node::helping_sink() {
	return _helps_outbuf_dst() ? _outbuf_dst->helping_sink() : this; }


bool Ipc_node::await_request(void * const buf_base,
                             size_t const buf_size)
{
	/* assertions */
	assert(_state == INACTIVE);

	/* prepare receipt of request */
	_inbuf.base = buf_base;
	_inbuf.size = buf_size;
	_inbuf.src  = 0;

	/* if anybody already announced a request receive it */
	if (!_request_queue.empty()) {
		_receive_request(_request_queue.dequeue());
		return true;
	}
	/* no request announced, so wait */
	_state = AWAIT_REQUEST;
	return false;
}


void Ipc_node::send_reply(void * const msg_base,
                          size_t const msg_size)
{
	/* reply to the last request if we have to */
	if (_state == PREPARE_REPLY) {
		if (_inbuf.src) {
			_inbuf.src->_receive_reply(msg_base, msg_size);
			_inbuf.src = 0;
		}
		_state = INACTIVE;
	}
}


void Ipc_node::cancel_waiting()
{
	switch (_state) {
	case AWAIT_REPLY:
		_cancel_outbuf_request();
		_state = INACTIVE;
		_send_request_failed();
		return;
	case AWAIT_REQUEST:
		_state = INACTIVE;
		_await_request_failed();
		return;
	case PREPARE_AND_AWAIT_REPLY:
		_cancel_outbuf_request();
		_state = PREPARE_REPLY;
		_send_request_failed();
		return;
	default: return;
	}
}


Ipc_node::Ipc_node() : _state(INACTIVE)
{
	_inbuf.src  = 0;
	_outbuf_dst = 0;
}


Ipc_node::~Ipc_node()
{
	_cancel_request_queue();
	_cancel_inbuf_request();
	_cancel_outbuf_request();
}
