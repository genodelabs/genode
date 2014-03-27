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

/* Genode includes */
#include <util/fifo.h>

/* core includes */
#include <assert.h>

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

		typedef Genode::Fifo<Message_buf> Message_fifo;

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
		State        _state;

		/**
		 * Buffer next request from request queue in 'r' to handle it
		 */
		void _receive_request(Message_buf * const r)
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

		/**
		 * Receive a given reply if one is expected
		 *
		 * \param base  base of the reply payload
		 * \param size  size of the reply payload
		 */
		void _receive_reply(void * const base, size_t const size)
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
			_await_ipc_succeeded(_inbuf.size);
		}

		/**
		 * Insert 'r' into request queue, buffer it if we were waiting for it
		 */
		void _announce_request(Message_buf * const r)
		{
			/* directly receive request if we've awaited it */
			if (_state == AWAIT_REQUEST) {
				_receive_request(r);
				_await_ipc_succeeded(_inbuf.size);
				return;
			}
			/* cannot receive yet, so queue request */
			_request_queue.enqueue(r);
		}

		/**
		 * Cancel all requests in request queue
		 */
		void _cancel_request_queue()
		{
			while (1) {
				Message_buf * const r = _request_queue.dequeue();
				if (!r) { return; }
				r->src->_outbuf_request_cancelled();
			}
		}

		/**
		 * Cancel request in outgoing buffer
		 */
		void _cancel_outbuf_request()
		{
			if (_outbuf_dst) {
				_outbuf_dst->_announced_request_cancelled(&_outbuf);
				_outbuf_dst = 0;
			}
		}

		/**
		 * Cancel request in incoming buffer
		 */
		void _cancel_inbuf_request()
		{
			if (_inbuf.src) {
				_inbuf.src->_outbuf_request_cancelled();
				_inbuf.src = 0;
			}
		}

		/**
		 * A request 'r' in inbuf or request queue was cancelled by sender
		 */
		void _announced_request_cancelled(Message_buf * const r)
		{
			if (_inbuf.src == r->src) {
				_inbuf.src = 0;
				return;
			}
			_request_queue.remove(r);
		}

		/**
		 * The request in the outbuf was cancelled by receiver
		 */
		void _outbuf_request_cancelled()
		{
			if (_outbuf_dst) {
				_outbuf_dst = 0;
				if (!_inbuf.src) { _state = INACTIVE; }
				else { _state = PREPARE_REPLY; }
				_await_ipc_failed();
			}
		}

		/**
		 * IPC node received a request without waiting
		 */
		virtual void _received_ipc_request(size_t const s) = 0;

		/**
		 * IPC node returned from waiting due to message receipt
		 *
		 * \param s  size of incoming message
		 */
		virtual void _await_ipc_succeeded(size_t const s) = 0;

		/**
		 * IPC node returned from waiting due to cancellation
		 */
		virtual void _await_ipc_failed() = 0;

	protected:

		/***************
		 ** Accessors **
		 ***************/

		Ipc_node * outbuf_dst() { return _outbuf_dst; }

		State state() { return _state; }

	public:

		/**
		 * Constructor
		 */
		Ipc_node() : _state(INACTIVE)
		{
			_inbuf.src  = 0;
			_outbuf_dst = 0;
		}

		/**
		 * Send a request and wait for the according reply
		 *
		 * \param dest        targeted IPC node
		 * \param req_base    base of the request payload
		 * \param req_size    size of the request payload
		 * \param inbuf_base  base of the reply buffer
		 * \param inbuf_size  size of the reply buffer
		 */
		void send_request_await_reply(Ipc_node * const dst,
		                              void * const     req_base,
		                              size_t const     req_size,
		                              void * const     inbuf_base,
		                              size_t const     inbuf_size)
		{
			/* assertions */
			assert(_state == INACTIVE || _state == PREPARE_REPLY);

			/* prepare transmission of request message */
			_outbuf.base = req_base;
			_outbuf.size = req_size;
			_outbuf.src  = this;
			_outbuf_dst  = dst;

			/* prepare reception of reply message */
			_inbuf.base = inbuf_base;
			_inbuf.size = inbuf_size;
			/* don't clear '_inbuf.origin' because we might prepare a reply */

			/* update state */
			if (_state != PREPARE_REPLY) { _state = AWAIT_REPLY; }
			else { _state = PREPARE_AND_AWAIT_REPLY; }

			/* announce request */
			dst->_announce_request(&_outbuf);
		}

		/**
		 * Wait until a request has arrived and load it for handling
		 *
		 * \param inbuf_base  base of the request buffer
		 * \param inbuf_size  size of the request buffer
		 *
		 * \return  wether a request could be received already
		 */
		bool await_request(void * const inbuf_base,
		                   size_t const inbuf_size)
		{
			/* assertions */
			assert(_state == INACTIVE);

			/* prepare receipt of request */
			_inbuf.base = inbuf_base;
			_inbuf.size = inbuf_size;
			_inbuf.src  = 0;

			/* if anybody already announced a request receive it */
			if (!_request_queue.empty()) {
				_receive_request(_request_queue.dequeue());
				_received_ipc_request(_inbuf.size);
				return true;
			}
			/* no request announced, so wait */
			_state = AWAIT_REQUEST;
			return false;
		}

		/**
		 * Reply to last request if there's any
		 *
		 * \param reply_base  base of the reply payload
		 * \param reply_size  size of the reply payload
		 */
		void send_reply(void * const reply_base,
		                size_t const reply_size)
		{
			/* reply to the last request if we have to */
			if (_state == PREPARE_REPLY) {
				if (_inbuf.src) {
					_inbuf.src->_receive_reply(reply_base, reply_size);
					_inbuf.src = 0;
				}
				_state = INACTIVE;
			}
		}

		/**
		 * Destructor
		 */
		~Ipc_node()
		{
			_cancel_request_queue();
			_cancel_inbuf_request();
			_cancel_outbuf_request();
		}

		/**
		 * If IPC node waits, cancel '_outbuf' to stop waiting
		 */
		void cancel_waiting()
		{
			switch (_state) {
			case AWAIT_REPLY:
				_cancel_outbuf_request();
				_state = INACTIVE;
				_await_ipc_failed();
				return;
			case AWAIT_REQUEST:
				_state = INACTIVE;
				_await_ipc_failed();
				return;
			case PREPARE_AND_AWAIT_REPLY:
				_cancel_outbuf_request();
				_state = PREPARE_REPLY;
				_await_ipc_failed();
				return;
			default: return;
			}
		}
};

#endif /* _KERNEL__IPC_NODE_H_ */
