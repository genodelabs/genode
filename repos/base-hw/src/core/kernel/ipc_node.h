/*
 * \brief   Backend for end points of synchronous interprocess communication
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__IPC_NODE_H_
#define _CORE__KERNEL__IPC_NODE_H_

/* Genode includes */
#include <util/construct_at.h>

/* base-local includes */
#include <base/internal/native_utcb.h>

/* core includes */
#include <kernel/fifo.h>
#include <kernel/interface.h>

namespace Genode { class Msgbuf_base; };

namespace Kernel
{
	class Pd;

	/**
	 * Backend for end points of synchronous interprocess communication
	 */
	class Ipc_node;

	using Ipc_node_queue = Kernel::Fifo<Ipc_node>;
}

class Kernel::Ipc_node : private Ipc_node_queue::Element
{
	protected:

		enum State
		{
			INACTIVE      = 1,
			AWAIT_REPLY   = 2,
			AWAIT_REQUEST = 3,
		};

		void _init(Genode::Native_utcb * utcb, Ipc_node * callee);

	private:

		friend class Core_thread;
		friend class Kernel::Fifo<Ipc_node>;
		friend class Genode::Fifo<Ipc_node>;

		State                 _state    = INACTIVE;
		capid_t               _capid    = cap_id_invalid();
		Ipc_node *            _caller   = nullptr;
		Ipc_node *            _callee   = nullptr;
		bool                  _help     = false;
		size_t                _rcv_caps = 0; /* max capability num to receive */
		Genode::Native_utcb * _utcb     = nullptr;
		Ipc_node_queue        _request_queue { };

		/* pre-allocation array for obkject identity references */
		void * _obj_id_ref_ptr[Genode::Msgbuf_base::MAX_CAPS_PER_MSG];

		inline void copy_msg(Ipc_node * const sender);

		/**
		 * Buffer next request from request queue in 'r' to handle it
		 */
		void _receive_request(Ipc_node * const caller);

		/**
		 * Receive a given reply if one is expected
		 */
		void _receive_reply(Ipc_node * callee);

		/**
		 * Insert 'r' into request queue, buffer it if we were waiting for it
		 */
		void _announce_request(Ipc_node * const node);

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
		void _announced_request_cancelled(Ipc_node * const node);

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

		Pd * _pd = nullptr; /* pointer to PD this IPC node is part of */


		/***************
		 ** Accessors **
		 ***************/

		Ipc_node * callee() { return _callee; }
		State      state()  { return _state;  }

	public:

		virtual ~Ipc_node();

		/**
		 * Send a request and wait for the according reply
		 *
		 * \param callee    targeted IPC node
		 * \param help      wether the request implies a helping relationship
		 */
		void send_request(Ipc_node * const callee, capid_t capid, bool help,
		                  unsigned rcv_caps);

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
			if (_caller && _caller->_help) f(_caller);

			/* call 'f' for each helper in our request queue */
			_request_queue.for_each([f] (Ipc_node * const node) {
				if (node->_help) f(node); });
		}

		/**
		 * Wait until a request has arrived and load it for handling
		 *
		 * \return  wether a request could be received already
		 */
		bool await_request(unsigned rcv_caps);

		/**
		 * Reply to last request if there's any
		 */
		void send_reply();

		/**
		 * If IPC node waits, cancel '_outbuf' to stop waiting
		 */
		void cancel_waiting();


		/***************
		 ** Accessors **
		 ***************/

		Pd                  *pd() const { return _pd; }
		Genode::Native_utcb *utcb()     { return _utcb; }
};

#endif /* _CORE__KERNEL__IPC_NODE_H_ */
