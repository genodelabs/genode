/*
 * \brief   Kernel representation of a user thread
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__KERNEL__THREAD_H_
#define _CORE__KERNEL__THREAD_H_

/* Genode includes */
#include <util/fifo.h>

/* core includes */
#include <cpu.h>
#include <tlb.h>
#include <pic.h>
#include <timer.h>
#include <assert.h>
#include <kernel/configuration.h>
#include <kernel/scheduler.h>
#include <kernel/object.h>

namespace Genode
{
	class Platform_thread;
}

namespace Kernel
{
	typedef Genode::Cpu             Cpu;
	typedef Genode::Page_flags      Page_flags;
	typedef Genode::Core_tlb        Core_tlb;
	typedef Genode::Signal          Signal;
	typedef Genode::Pagefault       Pagefault;
	typedef Genode::Native_utcb     Native_utcb;

	template <typename T> class Fifo     : public Genode::Fifo<T> { };

	class Schedule_context;
	typedef Scheduler<Schedule_context> Cpu_scheduler;

	/**
	 * Kernel object that can be scheduled for the CPU
	 */
	class Schedule_context : public Cpu_scheduler::Item
	{
		public:

			virtual void handle_exception() = 0;
			virtual void proceed() = 0;
	};

	/**
	 * Sends requests to other IPC nodes, accumulates request announcments,
	 * provides serial access to them and replies to them if expected.
	 * Synchronizes communication.
	 *
	 * IPC node states:
	 *
	 *         +----------+                               +---------------+                             +---------------+
	 * --new-->| inactive |---send-request-await-reply--->| await reply   |               +--send-note--| prepare reply |
	 *         |          |<--receive-reply---------------|               |               |             |               |
	 *         |          |<--cancel-waiting--------------|               |               |             |               |
	 *         |          |                               +---------------+               +------------>|               |
	 *         |          |<--request-is-a-note-------+---request-is-not-a-note------------------------>|               |
	 *         |          |<--------------------------(---not-await-request---+                         |               |
	 *         |          |                           |   +---------------+   |                         |               |
	 *         |          |---await-request-----------+-->| await request |<--+--send-reply-------------|               |
	 *         |          |<--cancel-waiting--------------|               |------announce-request--+--->|               |
	 *         |          |---send-reply---------+----+-->|               |                        |    |               |
	 *         |          |---send-note--+       |    |   +---------------+                        |    |               |
	 *         |          |              |       |    |                                            |    |               |
	 *         |          |<-------------+       |  request available                              |    |               |
	 *         |          |<--not-await-request--+    |                                            |    |               |
	 *         |          |<--request-is-a-note-------+-------------------request-is-not-a-note----(--->|               |
	 *         |          |<--request-is-a-note----------------------------------------------------+    |               |
	 *         +----------+                 +-------------------------+                                 |               |
	 *                                      | prepare and await reply |<--send-request-and-await-reply--|               |
	 *                                      |                         |---receive-reply---------------->|               |
	 *                                      |                         |---cancel-waiting--------------->|               |
	 *                                      +-------------------------+                                 +---------------+
	 *
	 * State model propagated to deriving classes:
	 *
	 *         +--------------+                                               +----------------+
	 * --new-->| has received |--send-request-await-reply-------------------->| awaits receipt |
	 *         |              |--await-request----------------------------+-->|                |
	 *         |              |                                           |   |                |
	 *         |              |<--request-available-----------------------+   |                |
	 *         |              |--send-reply-------------------------------+-->|                |
	 *         |              |--send-note--+                             |   |                |
	 *         |              |             |                             |   |                |
	 *         |              |<------------+                             |   |                |
	 *         |              |<--request-available-or-not-await-request--+   |                |
	 *         |              |<--announce-request----------------------------|                |
	 *         |              |<--receive-reply-------------------------------|                |
	 *         |              |<--cancel-waiting------------------------------|                |
	 *         +--------------+                                               +----------------+
	 */
	class Ipc_node
	{
		/**
		 * IPC node states as depicted initially
		 */
		enum State
		{
			INACTIVE = 1,
			AWAIT_REPLY = 2,
			AWAIT_REQUEST = 3,
			PREPARE_REPLY = 4,
			PREPARE_AND_AWAIT_REPLY = 5,
		};

		/**
		 * Describes the buffer for incoming or outgoing messages
		 */
		struct Message_buf : public Fifo<Message_buf>::Element
		{
			void * base;
			size_t size;
			Ipc_node * origin;
		};

		Fifo<Message_buf> _request_queue; /* requests that waits to be
		                                   * received by us */
		Message_buf _inbuf;  /* buffers message we have received lastly */
		Message_buf _outbuf; /* buffers the message we aim to send */
		State       _state;  /* current node state */

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
		 * Wether we expect to receive a reply message
		 */
		bool _awaits_reply();

		/**
		 * IPC node waits for a message to receive to its inbuffer
		 */
		virtual void _awaits_receipt() = 0;

		/**
		 * IPC node has received a message in its inbuffer
		 *
		 * \param s  size of the message
		 */
		virtual void _has_received(size_t const s) = 0;

		public:

			/**
			 * Construct an initially inactive IPC node
			 */
			Ipc_node();

			/**
			 * Destructor
			 */
			virtual ~Ipc_node() { }

			/**
			 * Send a request and wait for the according reply
			 *
			 * \param dest        targeted IPC node
			 * \param req_base    base of the request payload
			 * \param req_size    size of the request payload
			 * \param inbuf_base  base of the reply buffer
			 * \param inbuf_size  size of the reply buffer
			 */
			void send_request_await_reply(Ipc_node * const dest,
			                              void * const req_base,
			                              size_t const req_size,
			                              void * const inbuf_base,
			                              size_t const inbuf_size);

			/**
			 * Wait until a request has arrived and load it for handling
			 *
			 * \param inbuf_base  base of the request buffer
			 * \param inbuf_size  size of the request buffer
			 */
			void await_request(void * const inbuf_base,
			                   size_t const inbuf_size);

			/**
			 * Reply to last request if there's any
			 *
			 * \param reply_base  base of the reply payload
			 * \param reply_size  size of the reply payload
			 */
			void send_reply(void * const reply_base,
			                size_t const reply_size);

			/**
			 * Send a notification and stay inactive
			 *
			 * \param dest        targeted IPC node
			 * \param note_base   base of the note payload
			 * \param note_size   size of the note payload
			 *
			 * The caller must ensure that the note payload remains
			 * until it is buffered by the targeted node.
			 */
			void send_note(Ipc_node * const dest,
			               void * const note_base,
			               size_t const note_size);

			/**
			 * Stop waiting for a receipt if in a waiting state
			 */
			void cancel_waiting();
	};

	/**
	 * Exclusive ownership and handling of one IRQ per instance at a max
	 */
	class Irq_owner : public Object_pool<Irq_owner>::Item
	{
		/**
		 * To get any instance of this class by its ID
		 */
		typedef Object_pool<Irq_owner> Pool;
		static Pool * _pool() { static Pool _pool; return &_pool; }

		/**
		 * Is called when the IRQ we were waiting for has occured
		 */
		virtual void _received_irq() = 0;

		/**
		 * Is called when we start waiting for the occurence of an IRQ
		 */
		virtual void _awaits_irq() = 0;

		public:

			/**
			 * Translate 'Irq_owner_pool'-item ID to IRQ ID
			 */
			static unsigned id_to_irq(unsigned id) { return id - 1; }

			/**
			 * Translate IRQ ID to 'Irq_owner_pool'-item ID
			 */
			static unsigned irq_to_id(unsigned irq) { return irq + 1; }

			/**
			 * Constructor
			 */
			Irq_owner() : Pool::Item(0) { }

			/**
			 * Destructor
			 */
			virtual ~Irq_owner() { }

			/**
			 * Ensure that our 'receive_irq' gets called on IRQ 'irq'
			 *
			 * \return  wether the IRQ is allocated to the caller or not
			 */
			bool allocate_irq(unsigned const irq);

			/**
			 * Release the ownership of the IRQ 'irq' if we own it
			 *
			 * \return  wether the IRQ is freed or not
			 */
			bool free_irq(unsigned const irq);

			/**
			 * If we own an IRQ, enable it and await 'receive_irq'
			 */
			void await_irq();

			/**
			 * Stop waiting for an IRQ if in a waiting state
			 */
			void cancel_waiting();

			/**
			 * Denote occurence of an IRQ if we own it and awaited it
			 */
			void receive_irq(unsigned const irq);

			/**
			 * Get owner of IRQ or 0 if the IRQ is not owned by anyone
			 */
			static Irq_owner * owner(unsigned irq);
	};

	/**
	 * Kernel representation of a user thread
	 */
	class Thread : public Cpu::User_context,
	               public Object<Thread, MAX_THREADS>,
	               public Schedule_context,
	               public Fifo<Thread>::Element,
	               public Ipc_node,
	               public Irq_owner
	{
		enum State
		{
			SCHEDULED,
			AWAIT_START,
			AWAIT_IPC,
			AWAIT_RESUMPTION,
			AWAIT_IRQ,
			AWAIT_SIGNAL,
			AWAIT_SIGNAL_CONTEXT_DESTRUCT,
			CRASHED,
		};

		Platform_thread * const _platform_thread; /* userland object wich
		                                           * addresses this thread */
		State _state; /* thread state, description given at the beginning */
		Pagefault _pagefault; /* last pagefault triggered by this thread */
		Thread * _pager; /* gets informed if thread throws a pagefault */
		unsigned _pd_id; /* ID of the PD this thread runs on */
		Native_utcb * _phys_utcb; /* physical UTCB base */
		Native_utcb * _virt_utcb; /* virtual UTCB base */
		Signal_receiver * _signal_receiver; /* receiver we are currently
		                                     * listen to */

		/**
		 * Resume execution
		 */
		void _schedule();


		/**************
		 ** Ipc_node **
		 **************/

		void _has_received(size_t const s);

		void _awaits_receipt();


		/***************
		 ** Irq_owner **
		 ***************/

		void _received_irq();

		void _awaits_irq();

		public:

			void * operator new (size_t, void * p) { return p; }

			/**
			 * Constructor
			 */
			Thread(Platform_thread * const platform_thread) :
				_platform_thread(platform_thread),
				_state(AWAIT_START), _pager(0), _pd_id(0),
				_phys_utcb(0), _virt_utcb(0), _signal_receiver(0)
			{ }

			/**
			 * Suspend the thread due to unrecoverable misbehavior
			 */
			void crash();

			/**
			 * Prepare thread to get scheduled the first time
			 *
			 * \param ip         initial instruction pointer
			 * \param sp         initial stack pointer
			 * \param cpu_id     target cpu
			 * \param pd_id      target protection-domain
			 * \param utcb_phys  physical UTCB pointer
			 * \param utcb_virt  virtual UTCB pointer
			 */
			void prepare_to_start(void * const        ip,
			                      void * const        sp,
			                      unsigned const      cpu_id,
			                      unsigned const      pd_id,
			                      Native_utcb * const utcb_phys,
			                      Native_utcb * const utcb_virt);

			/**
			 * Start this thread
			 *
			 * \param ip         initial instruction pointer
			 * \param sp         initial stack pointer
			 * \param cpu_id     target cpu
			 * \param pd_id      target protection-domain
			 * \param utcb_phys  physical UTCB pointer
			 * \param utcb_virt  virtual UTCB pointer
			 */
			void start(void * const        ip,
			           void * const        sp,
			           unsigned const      cpu_id,
			           unsigned const      pd_id,
			           Native_utcb * const utcb_phys,
			           Native_utcb * const utcb_virt);

			/**
			 * Pause this thread
			 */
			void pause();

			/**
			 * Stop this thread
			 */
			void stop();

			/**
			 * Resume this thread
			 */
			int resume();

			/**
			 * Send a request and await the reply
			 */
			void request_and_wait(Thread * const dest, size_t const size);

			/**
			 * Wait for any request
			 */
			void wait_for_request();

			/**
			 * Reply to the last request
			 */
			void reply(size_t const size, bool const await_request);

			/**
			 * Handle a pagefault that originates from this thread
			 *
			 * \param va  virtual fault address
			 * \param w   if fault was caused by a write access
			 */
			void pagefault(addr_t const va, bool const w);

			/**
			 * Get unique thread ID, avoid method ambiguousness
			 */
			unsigned id() const { return Object::id(); }

			/**
			 * Gets called when we await a signal at 'receiver'
			 */
			void await_signal(Kernel::Signal_receiver * receiver);

			/**
			 * Gets called when we have received a signal at a signal receiver
			 */
			void received_signal();

			/**
			 * Handle the exception that currently blocks this thread
			 */
			void handle_exception();

			/**
			 * Continue executing this thread in userland
			 */
			void proceed();

			void kill_signal_context_blocks();

			void kill_signal_context_done();

			/***************
			 ** Accessors **
			 ***************/

			Platform_thread * platform_thread() const {
				return _platform_thread; }

			void pager(Thread * const p) { _pager = p; }

			unsigned pd_id() const { return _pd_id; }

			Native_utcb * phys_utcb() const { return _phys_utcb; }
	};
}

#endif /* _CORE__KERNEL__THREAD_H_ */

