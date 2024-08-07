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
#include <kernel/thread.h>

using namespace Kernel;


void Ipc_node::_receive_from(Ipc_node &node)
{
	_thread.ipc_copy_msg(node._thread);
	_in.state = In::REPLY;
}


void Ipc_node::_cancel_send()
{
	if (_out.node) {

		/*
		 * If the receiver is already processing our message,
		 * we have to ensure that he skips sending a reply by
		 * letting his node state indicate our withdrawal.
		 */
		if (_out.node->_in.state == In::REPLY)
			_out.node->_in.queue.head([&] (Queue_item &item) {
				if (&item == &_queue_item)
					_out.node->_in.state = In::REPLY_NO_SENDER;
			});

		_out.node->_in.queue.remove(_queue_item);
		_out.node = nullptr;
	}
	if (_out.sending()) {
		_thread.ipc_send_request_failed();
		_out.state = Out::READY;
	}
}


bool Ipc_node::ready_to_send() const
{
	return _out.state == Out::READY && !_in.waiting();
}


void Ipc_node::send(Ipc_node &node)
{
	node._in.queue.enqueue(_queue_item);

	if (node._in.waiting()) {
		node._receive_from(*this);
		node._thread.ipc_await_request_succeeded();
	}
	_out.node  = &node;
	_out.state = Out::SEND;
}


bool Ipc_node::ready_to_wait() const
{
	return _in.state == In::READY;
}


void Ipc_node::wait()
{
	_in.state = In::WAIT;
	_in.queue.head([&] (Queue_item &item) {
		_receive_from(item.object());
	});
}


void Ipc_node::reply()
{
	if (_in.state == In::REPLY)
		_in.queue.dequeue([&] (Queue_item &item) {
			Ipc_node &node { item.object() };
			node._thread.ipc_copy_msg(_thread);
			node._out.node  = nullptr;
			node._out.state = Out::READY;
			node._thread.ipc_send_request_succeeded();
		});

	_in.state = In::READY;
}


void Ipc_node::cancel_waiting()
{
	if (_out.sending())
		_cancel_send();

	if (_in.waiting()) {
		_in.state = In::READY;
		_thread.ipc_await_request_failed();
	}
}


Ipc_node::Ipc_node(Thread &thread)
:
	_thread(thread)
{ }


Ipc_node::~Ipc_node()
{
	_in.state  = In::DESTRUCT;
	_out.state = Out::DESTRUCT;

	_cancel_send();

	_in.queue.for_each([&] (Queue_item &item) {
		item.object()._cancel_send();
	});
}
