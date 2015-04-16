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

#ifndef _KERNEL__IPC_NODE_H_
#define _KERNEL__IPC_NODE_H_

/* core includes */
#include <kernel/fifo.h>
#include <kernel/interface.h>

namespace Kernel
{
	/**
	 * Backend for end points of synchronous interprocess communication
	 */
	class Ipc_node;
}

class Kernel::Ipc_node
{
	protected:

		enum State
		{
			INACTIVE                = 1,
			AWAIT_REPLY             = 2,
			AWAIT_REQUEST           = 3,
			PREPARE_REPLY           = 4,
			PREPARE_AND_AWAIT_REPLY = 5,
		};

	private:

		class Message_buf;

		typedef Kernel::Fifo<Message_buf> Message_fifo;

		/**
		 * Describes the buffer for incoming or outgoing messages
		 */
		class Message_buf : public Message_fifo::Element
		{
			public:

				void *     base;
				size_t     size;
				Ipc_node * src;
		};

		Message_fifo _request_queue;
		Message_buf  _inbuf;
		Message_buf  _outbuf;
		Ipc_node *   _outbuf_dst;
		bool         _outbuf_dst_help;
		State        _state;

		/**
		 * Buffer next request from request queue in 'r' to handle it
		 */
		void _receive_request(Message_buf * const r);

		/**
		 * Receive a given reply if one is expected
		 *
		 * \param base  base of the reply payload
		 * \param size  size of the reply payload
		 */
		void _receive_reply(void * const base, size_t const size);

		/**
		 * Insert 'r' into request queue, buffer it if we were waiting for it
		 */
		void _announce_request(Message_buf * const r);

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
		void _announced_request_cancelled(Message_buf * const r);

		/**
		 * The request in the outbuf was cancelled by receiver
		 */
		void _outbuf_request_cancelled();

		/**
		 * Return wether we are the source of a helping relationship
		 */
		bool _helps_outbuf_dst();

		/**
		 * IPC node returned from waiting due to reply receipt
		 */
		virtual void _send_request_succeeded() = 0;

		/**
		 * IPC node returned from waiting due to reply cancellation
		 */
		virtual void _send_request_failed() = 0;

		/**
		 * IPC node returned from waiting due to request receipt
		 */
		virtual void _await_request_succeeded() = 0;

		/**
		 * IPC node returned from waiting due to request cancellation
		 */
		virtual void _await_request_failed() = 0;

	protected:

		/***************
		 ** Accessors **
		 ***************/

		Ipc_node * outbuf_dst() { return _outbuf_dst; }

		State state() { return _state; }

	public:

		Ipc_node();
		~Ipc_node();

		/**
		 * Send a request and wait for the according reply
		 *
		 * \param dst       targeted IPC node
		 * \param buf_base  base of receive buffer and request message
		 * \param buf_size  size of receive buffer
		 * \param msg_size  size of request message
		 * \param help      wether the request implies a helping relationship
		 */
		void send_request(Ipc_node * const dst, void * const buf_base,
		                  size_t const buf_size, size_t const msg_size,
		                  bool help);

		/**
		 * Return root destination of the helping-relation tree we are in
		 */
		Ipc_node * helping_sink();

		/**
		 * Call function 'f' of type 'void (Ipc_node *)' for each helper
		 */
		template <typename F> void for_each_helper(F f)
		{
			/* if we have a helper in the receive buffer, call 'f' for it */
			if (_state == PREPARE_REPLY || _state == PREPARE_AND_AWAIT_REPLY) {
				if (_inbuf.src->_outbuf_dst_help) { f(_inbuf.src); } }

			/* call 'f' for each helper in our request queue */
			_request_queue.for_each([f] (Message_buf * const b) {
				if (b->src->_outbuf_dst_help) { f(b->src); } });
		}

		/**
		 * Wait until a request has arrived and load it for handling
		 *
		 * \param buf_base  base of receive buffer
		 * \param buf_size  size of receive buffer
		 *
		 * \return  wether a request could be received already
		 */
		bool await_request(void * const buf_base,
		                   size_t const buf_size);

		/**
		 * Reply to last request if there's any
		 *
		 * \param msg_base  base of reply message
		 * \param msg_size  size of reply message
		 */
		void send_reply(void * const msg_base,
		                size_t const msg_size);

		/**
		 * If IPC node waits, cancel '_outbuf' to stop waiting
		 */
		void cancel_waiting();
};

#endif /* _KERNEL__IPC_NODE_H_ */
