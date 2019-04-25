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

#ifndef _CORE__KERNEL__IPC_NODE_H_
#define _CORE__KERNEL__IPC_NODE_H_

/* Genode includes */
#include <util/fifo.h>

namespace Kernel
{
	class Thread;

	/**
	 * Backend for end points of synchronous interprocess communication
	 */
	class Ipc_node;
}

class Kernel::Ipc_node
{
	private:

		using Queue_item = Genode::Fifo_element<Ipc_node>;
		using Queue      = Genode::Fifo<Queue_item>;

		enum State
		{
			INACTIVE      = 1,
			AWAIT_REPLY   = 2,
			AWAIT_REQUEST = 3,
		};

		Thread     &_thread;
		Queue_item  _request_queue_item { *this };
		State       _state              { INACTIVE };
		Ipc_node   *_caller             { nullptr };
		Ipc_node   *_callee             { nullptr };
		bool        _help               { false };
		Queue       _request_queue      { };

		/**
		 * Buffer next request from request queue in 'r' to handle it
		 */
		void _receive_request(Ipc_node &caller);

		/**
		 * Receive a given reply if one is expected
		 */
		void _receive_reply(Ipc_node &callee);

		/**
		 * Insert 'r' into request queue, buffer it if we were waiting for it
		 */
		void _announce_request(Ipc_node &node);

		/**
		 * Cancel all requests in request queue
		 */
		void _cancel_request_queue();

		/**
		 * Cancel request in outgoing buffer
		 */
		void _cancel_outbuf_request();

		/**
		 * Cancel request in incoming buffer
		 */
		void _cancel_inbuf_request();

		/**
		 * A request 'r' in inbuf or request queue was cancelled by sender
		 */
		void _announced_request_cancelled(Ipc_node &node);

		/**
		 * The request in the outbuf was cancelled by receiver
		 */
		void _outbuf_request_cancelled();

		/**
		 * Return wether we are the source of a helping relationship
		 */
		bool _helps_outbuf_dst();

		/**
		 * Make the class noncopyable because it has pointer members
		 */
		Ipc_node(const Ipc_node&) = delete;

		/**
		 * Make the class noncopyable because it has pointer members
		 */
		const Ipc_node& operator=(const Ipc_node&) = delete;

	public:

		/**
		 * Destructor
		 */
		~Ipc_node();

		/**
		 * Constructor
		 */
		Ipc_node(Thread &thread);

		/**
		 * Send a request and wait for the according reply
		 *
		 * \param callee    targeted IPC node
		 * \param help      wether the request implies a helping relationship
		 */
		bool can_send_request();
		void send_request(Ipc_node &callee,
		                  bool      help);

		/**
		 * Return root destination of the helping-relation tree we are in
		 */
		Thread &helping_sink();

		/**
		 * Call function 'f' of type 'void (Ipc_node *)' for each helper
		 */
		template <typename F> void for_each_helper(F f)
		{
			/* if we have a helper in the receive buffer, call 'f' for it */
			if (_caller && _caller->_help) f(_caller->_thread);

			/* call 'f' for each helper in our request queue */
			_request_queue.for_each([f] (Queue_item &item) {
				Ipc_node &node { item.object() };
				if (node._help) f(node._thread);
			});
		}

		/**
		 * Wait until a request has arrived and load it for handling
		 *
		 * \return  wether a request could be received already
		 */
		bool can_await_request();
		void await_request();

		/**
		 * Reply to last request if there's any
		 */
		void send_reply();

		/**
		 * If IPC node waits, cancel '_outbuf' to stop waiting
		 */
		void cancel_waiting();

		bool awaits_request() const { return _state == AWAIT_REQUEST; }
};

#endif /* _CORE__KERNEL__IPC_NODE_H_ */
