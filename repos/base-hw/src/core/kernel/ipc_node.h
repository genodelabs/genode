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

namespace Kernel {

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

		struct In
		{
			enum State { READY, WAIT, REPLY, REPLY_NO_SENDER, DESTRUCT };

			State state { READY };
			Queue queue { };

			bool waiting() const
			{
				return state == WAIT;
			}
		};

		struct Out
		{
			enum State { READY, SEND, SEND_HELPING, DESTRUCT };

			State     state { READY   };
			Ipc_node *node  { nullptr };

			bool sending() const
			{
				return state == SEND_HELPING || state == SEND;
			}
		};

		Thread     &_thread;
		Queue_item  _queue_item { *this };
		Out         _out        { };
		In          _in         { };

		/**
		 * Receive a message from another IPC node
		 */
		void _receive_from(Ipc_node &node);

		/**
		 * Cancel an ongoing send operation
		 */
		void _cancel_send();

		/**
		 * Return wether this IPC node is helping another one
		 */
		bool _helping() const;

		/**
		 * Noncopyable
		 */
		Ipc_node(const Ipc_node&) = delete;
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
		 * \param node  targeted IPC node
		 * \param help  wether the request implies a helping relationship
		 */
		bool can_send_request() const;
		void send_request(Ipc_node &node,
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
			_in.queue.for_each([f] (Queue_item &item) {
				Ipc_node &node { item.object() };

				if (node._helping())
					f(node._thread);
			});
		}

		/**
		 * Wait until a request has arrived and load it for handling
		 *
		 * \return  wether a request could be received already
		 */
		bool can_await_request() const;
		void await_request();

		/**
		 * Reply to last request if there's any
		 */
		void send_reply();

		/**
		 * If IPC node waits, cancel '_outbuf' to stop waiting
		 */
		void cancel_waiting();

		bool awaits_request() const { return _in.waiting(); }
};

#endif /* _CORE__KERNEL__IPC_NODE_H_ */
