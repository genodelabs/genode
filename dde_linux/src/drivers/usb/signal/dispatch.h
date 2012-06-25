/**
 * \brief  Packet-stream-session components
 * \author Sebastian Sumpf
 * \author Norman Feske
 * \date   2012-07-06
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SIGNAL__DISPATCHER_H_
#define _SIGNAL__DISPATCHER_H_

#include "signal.h"

template <typename T>
class Signal_dispatcher : public Driver_context,
                          public Genode::Signal_context_capability
{
	private:

		T &obj;
		void (T::*member) ();
		Genode::Signal_receiver *sig_rec;

	public:

		/**
		 * Constructor
		 *
		 * \param sig_rec     signal receiver to associate the signal
		 *                    handler with
		 * \param obj,member  object and member function to call when
		 *                    the signal occurs
		 */
		Signal_dispatcher(Genode::Signal_receiver *sig_rec,
		                  T &obj, void (T::*member)())
		:
			Genode::Signal_context_capability(sig_rec->manage(this)),
			obj(obj), member(member),
			sig_rec(sig_rec)
		{ }

		~Signal_dispatcher() { sig_rec->dissolve(this); }

		void handle() { (obj.*member)(); }
		char const *debug() { return "Signal_dispatcher"; }
};


/**
 * Session components that overrides signal handlers
 */
template <typename RPC>
class Packet_session_component : public RPC
{
	private:

		Signal_dispatcher<Packet_session_component> _process_packet_dispatcher;

	protected:

		virtual void _process_packets() = 0;

	public:

		Packet_session_component(Genode::Dataspace_capability  tx_ds,
		                         Genode::Rpc_entrypoint       &ep,
		                         Genode::Signal_receiver      *sig_rec)
		:
			RPC(tx_ds, ep),
			_process_packet_dispatcher(sig_rec, *this,
			                           &Packet_session_component::_process_packets)
		{ 
			/*
			 * Register '_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			RPC::_tx.sigh_packet_avail(_process_packet_dispatcher);
			RPC::_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}

		Packet_session_component(Genode::Dataspace_capability  tx_ds,
		                         Genode::Dataspace_capability  rx_ds,
		                         Genode::Range_allocator      *rx_buffer_alloc,
		                         Genode::Rpc_entrypoint       &ep,
		                         Genode::Signal_receiver      *sig_rec)
		:
			RPC(tx_ds, rx_ds, rx_buffer_alloc, ep),
			_process_packet_dispatcher(sig_rec, *this,
			                           &Packet_session_component::_process_packets)
		{ 
			/*
			 * Register '_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			RPC::_tx.sigh_packet_avail(_process_packet_dispatcher);
			RPC::_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}
};


	/**
	 * Abstract device
	 */
	struct Device { };


	/**
	 * Root component, handling new session requests
	 */
	template <typename ROOT_COMPONENT, typename SESSION_COMPONENT>
	class Packet_root : public ROOT_COMPONENT
	{
		private:

			Genode::Rpc_entrypoint  &_ep;
			Genode::Signal_receiver *_sig_rec;
			Device                  *_device;

		protected:

			/**
			 * Always returns the singleton block-session component
			 */
			SESSION_COMPONENT *_create_session(const char *args)
			{
				using namespace Genode;
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				size_t tx_buf_size =
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
				size_t rx_buf_size =
					Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

				/* delete ram quota by the memory needed for the session */
				size_t session_size = max((size_t)4096,
				                          sizeof(SESSION_COMPONENT)
				                          + sizeof(Allocator_avl));
				if (ram_quota < session_size)
					throw Root::Quota_exceeded();

				/*
				 * Check if donated ram quota suffices for both communication
				 * buffers. Also check both sizes separately to handle a
				 * possible overflow of the sum of both sizes.
				 */
				if (tx_buf_size > ram_quota - session_size) {
					PERR("insufficient 'ram_quota', got %zd, need %zd",
					     ram_quota, tx_buf_size + session_size);
					throw Root::Quota_exceeded();
				}

				return new (ROOT_COMPONENT::md_alloc())
					SESSION_COMPONENT(env()->ram_session()->alloc(tx_buf_size),
					                  env()->ram_session()->alloc(rx_buf_size),
					                  _ep, _sig_rec, _device);
			}

		public:

			Packet_root(Genode::Rpc_entrypoint  *session_ep, Genode::Allocator *md_alloc,
			            Genode::Signal_receiver *sig_rec, Device *device)
			:
				ROOT_COMPONENT(session_ep, md_alloc),
				_ep(*session_ep), _sig_rec(sig_rec), _device(device)
			{ }
	};

#endif /* _SIGNAL__DISPATCHER_H_ */
