/*
 * \brief   Backend for end points of synchronous interprocess communication
 * \author  Martin Stein
 * \author  Stefan Kalkowski
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
#include <base/native_types.h>

/* core includes */
#include <assert.h>
#include <platform_pd.h>
#include <kernel/ipc_node.h>
#include <kernel/pd.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>

using namespace Kernel;


void Ipc_node::copy_msg(Ipc_node * const sender)
{
	using namespace Genode;
	using Reference = Object_identity_reference;

	/* copy payload and set destination capability id */
	*_utcb = *sender->_utcb;
	_utcb->destination(sender->_capid);

	/* translate capabilities */
	for (unsigned i = 0; i < _rcv_caps; i++) {

		capid_t id = sender->_utcb->cap_get(i);

		/* if there is no capability to send, just free the pre-allocation */
		if (i >= sender->_utcb->cap_cnt()) {
			pd()->platform_pd()->capability_slab().free(_obj_id_ref_ptr[i]);
			continue;
		}

		/* within the same pd, we can simply copy the id */
		if (pd() == sender->pd()) {
			_utcb->cap_add(id);
			pd()->platform_pd()->capability_slab().free(_obj_id_ref_ptr[i]);
			continue;
		}

		/* lookup the capability id within the caller's cap space */
		Reference *oir = (id == cap_id_invalid())
			? nullptr : sender->pd()->cap_tree().find(id);

		/* if the caller's capability is invalid, free the pre-allocation */
		if (!oir) {
			_utcb->cap_add(cap_id_invalid());
			pd()->platform_pd()->capability_slab().free(_obj_id_ref_ptr[i]);
			continue;
		}

		/* lookup the capability id within the callee's cap space */
		Reference *dst_oir = oir->find(pd());

		/* if it is not found, and the target is not core, create a reference */
		if (!dst_oir && (pd() != core_pd())) {
			dst_oir = oir->factory(_obj_id_ref_ptr[i], *pd());
			if (!dst_oir)
				pd()->platform_pd()->capability_slab().free(_obj_id_ref_ptr[i]);
		} else /* otherwise free the pre-allocation */
			pd()->platform_pd()->capability_slab().free(_obj_id_ref_ptr[i]);

		/* add the translated capability id to the target buffer */
		_utcb->cap_add(dst_oir ? dst_oir->capid() : cap_id_invalid());
	}
}


void Ipc_node::_receive_request(Ipc_node * const caller)
{
	copy_msg(caller);
	_caller  = caller;
	_state   = PREPARE_REPLY;
}


void Ipc_node::_receive_reply(Ipc_node * callee)
{
	copy_msg(callee);
	_state = (_state != PREPARE_AND_AWAIT_REPLY) ? INACTIVE
	                                             : PREPARE_REPLY;
	_send_request_succeeded();
}


void Ipc_node::_announce_request(Ipc_node * const node)
{
	/* directly receive request if we've awaited it */
	if (_state == AWAIT_REQUEST) {
		_receive_request(node);
		_await_request_succeeded();
		return;
	}

	/* cannot receive yet, so queue request */
	_request_queue.enqueue(node);
}


void Ipc_node::_cancel_request_queue()
{
	Ipc_node * node;
	while ((node = _request_queue.dequeue()))
		node->_outbuf_request_cancelled();
}


void Ipc_node::_cancel_outbuf_request()
{
	if (_callee) {
		_callee->_announced_request_cancelled(this);
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


void Ipc_node::_announced_request_cancelled(Ipc_node * const node)
{
	if (_caller == node) _caller = nullptr;
	else _request_queue.remove(node);
}


void Ipc_node::_outbuf_request_cancelled()
{
	if (_callee == nullptr) return;

	_callee = nullptr;
	_state = (!_caller) ? INACTIVE : PREPARE_REPLY;
	_send_request_failed();
}


bool Ipc_node::_helps_outbuf_dst()
{
	return (_state == PREPARE_AND_AWAIT_REPLY ||
	        _state == AWAIT_REPLY) && _help;
}


void Ipc_node::_init(Genode::Native_utcb * utcb, Ipc_node * starter)
{
	_utcb = utcb;
	_rcv_caps = starter->_utcb->cap_cnt();
	Genode::Allocator &slab = pd()->platform_pd()->capability_slab();
	for (unsigned i = 0; i < _rcv_caps; i++)
		_obj_id_ref_ptr[i] = slab.alloc(sizeof(Object_identity_reference));
	copy_msg(starter);
}


void Ipc_node::send_request(Ipc_node * const callee, capid_t capid, bool help,
                            unsigned rcv_caps)
{
	/* assertions */
	assert(_state == INACTIVE || _state == PREPARE_REPLY);

	Genode::Allocator &slab = pd()->platform_pd()->capability_slab();
	for (unsigned i = 0; i < rcv_caps; i++)
		_obj_id_ref_ptr[i] = slab.alloc(sizeof(Object_identity_reference));

	/* update state */
	_state = (_state != PREPARE_REPLY) ? AWAIT_REPLY
	                                   : PREPARE_AND_AWAIT_REPLY;
	_callee   = callee;
	_capid    = capid;
	_help     = false;
	_rcv_caps = rcv_caps;

	/* announce request */
	_callee->_announce_request(this);

	_help = help;
}


Ipc_node * Ipc_node::helping_sink() {
	return _helps_outbuf_dst() ? _callee->helping_sink() : this; }


bool Ipc_node::await_request(unsigned rcv_caps)
{
	/* assertions */
	assert(_state == INACTIVE);

	Genode::Allocator &slab = pd()->platform_pd()->capability_slab();
	for (unsigned i = 0; i < rcv_caps; i++)
		_obj_id_ref_ptr[i] = slab.alloc(sizeof(Object_identity_reference));

	_rcv_caps = rcv_caps;

	/* if anybody already announced a request receive it */
	if (!_request_queue.empty()) {
		_receive_request(_request_queue.dequeue());
		return true;
	}

	/* no request announced, so wait */
	_state = AWAIT_REQUEST;
	return false;
}


void Ipc_node::send_reply()
{
	/* reply to the last request if we have to */
	if (_state == PREPARE_REPLY) {
		if (_caller != nullptr) {
			_caller->_receive_reply(this);
			_caller = nullptr;
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
		break;
	case AWAIT_REQUEST:
		_state = INACTIVE;
		_await_request_failed();
		break;
	case PREPARE_AND_AWAIT_REPLY:
		_cancel_outbuf_request();
		_state = PREPARE_REPLY;
		_send_request_failed();
		return;
	default: return;
	}
}


char const * Ipc_node::pd_label() const {
	return (_pd) ? _pd->platform_pd()->label() : "?"; }


Ipc_node::~Ipc_node()
{
	_cancel_request_queue();
	_cancel_inbuf_request();
	_cancel_outbuf_request();
}

