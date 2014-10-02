/*
 * \brief  Signal service
 * \author Norman Feske
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SIGNAL_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__SIGNAL_SESSION_COMPONENT_H_

#include <signal_session/signal_session.h>
#include <signal_session/source_rpc_object.h>
#include <base/allocator_guard.h>
#include <base/tslab.h>
#include <base/lock.h>
#include <base/rpc_client.h>
#include <base/rpc_server.h>
#include <util/fifo.h>
#include <base/signal.h>

namespace Genode {

	class Signal_source_component;
	class Signal_context_component;

	typedef Fifo<Signal_context_component> Signal_queue;

	class Signal_context_component : public Rpc_object<Signal_context>,
	                                 public Signal_queue::Element
	{
		private:

			long                     _imprint;
			int                      _cnt;
			Signal_source_component *_source;

		public:

			/**
			 * Constructor
			 */
			Signal_context_component(long imprint, Signal_source_component *source)
			: _imprint(imprint), _cnt(0), _source(source) { }

			/**
			 * De-constructor
			 */
			~Signal_context_component(); 

			/**
			 * Increment number of signals to be delivered at once
			 */
			void increment_signal_cnt(int increment) { _cnt += increment; }

			/**
			 * Reset number of pending signals
			 */
			void reset_signal_cnt() { _cnt = 0; }

			long                     imprint() { return _imprint; }
			int                      cnt()     { return _cnt; }
			Signal_source_component *source()  { return _source; }
	};


	class Signal_source_component : public Signal_source_rpc_object
	{
		/**
		 * Helper for clean destruction of signal-source component
		 *
		 * Normally, reply capabilities are implicitly destroyed when answering
		 * an RPC call. But when destructing a signal session while a signal-
		 * source client is blocking on a 'wait_for_signal' call, this
		 * blocking call will never return via the normal control flow
		 * (signal submission). In this case, the reply capability would
		 * outlive the signal session. To avoid the leakage of such reply
		 * capabilities, we let the signal-session destructor perform a
		 * core-local RPC call to the so-called 'Finalizer' object, which has
		 * the sole purpose of replying to the last outstanding
		 * 'wait_for_signal' call and thereby releasing the corresponding
		 * reply capability.
		 */
		struct Finalizer
		{
			GENODE_RPC(Rpc_exit, void, exit);
			GENODE_RPC_INTERFACE(Rpc_exit);
		};

		struct Finalizer_component : Rpc_object<Finalizer, Finalizer_component>
		{
			Signal_source_component &source;

			Finalizer_component(Signal_source_component &source)
			: source(source) { }

			void exit();
		};

		private:

			Signal_queue        _signal_queue;
			Rpc_entrypoint     *_entrypoint;
			Native_capability   _reply_cap;
			Finalizer_component _finalizer;
			Capability<Finalizer> _finalizer_cap;

		public:

			/**
			 * Constructor
			 */
			Signal_source_component(Rpc_entrypoint *rpc_entrypoint);

			~Signal_source_component();

			void release(Signal_context_component *context);

			void submit(Signal_context_component *context,
			            Ipc_ostream              *ostream,
			            int                       cnt);


			/*****************************
			 ** Signal-source interface **
			 *****************************/

			Signal wait_for_signal();
	};


	class Signal_session_component : public Rpc_object<Signal_session>
	{
		private:

			Rpc_entrypoint                       *_source_ep;
			Rpc_entrypoint                       *_context_ep;
			Signal_source_component               _source;
			Signal_source_capability              _source_cap;
			Allocator_guard                       _md_alloc;
			Tslab<Signal_context_component,
			      960*sizeof(long)>               _contexts_slab;
			Ipc_ostream                          *_ipc_ostream;

		public:

			/**
			 * Constructor
			 *
			 * \param source_ep    entrypoint holding signal-source component
			 *                     objects
			 * \param context_ep   global pool of all signal contexts
			 * \param md_alloc     backing-store allocator for
			 *                     signal-context component objects
			 *
			 * To maintain proper synchronization, 'signal_source_ep' must be
			 * the same entrypoint as used for the signal-session component.
			 * The 'signal_context_ep' is only used for associative array
			 * to map signal-context capabilities to 'Signal_context_component'
			 * objects and as capability allocator for such objects.
			 */
			Signal_session_component(Rpc_entrypoint *source_ep,
			                         Rpc_entrypoint *context_ep,
			                         Allocator      *context_md_alloc,
			                         size_t          ram_quota);

			~Signal_session_component();

			/**
			 * Register quota donation at allocator guard
			 */
			void upgrade_ram_quota(size_t ram_quota) { _md_alloc.upgrade(ram_quota); }


			/******************************
			 ** Signal-session interface **
			 ******************************/

			Signal_source_capability signal_source();
			Signal_context_capability alloc_context(long imprint);
			void free_context(Signal_context_capability context_cap);
			void submit(Signal_context_capability context_cap, unsigned cnt);


			/**************************
			 ** Rpc_object interface **
			 **************************/

			Rpc_exception_code dispatch(int opcode, Ipc_istream &is, Ipc_ostream &os)
			{
				/*
				 * Make IPC output stream available to the submit function. The
				 * stream is used to carry signal payload for the out-of-order
				 * handling of 'wait_for_signal' replies.
				 */
				_ipc_ostream = &os;

				/* dispatch RPC */
				return Rpc_object<Signal_session>::dispatch(opcode, is, os);
			}
	};
}

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
