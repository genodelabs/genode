/**
 * \brief  USB session client implementation
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB_SESSION__CLIENT_H_
#define _INCLUDE__USB_SESSION__CLIENT_H_

#include <base/rpc_client.h>

#include <packet_stream_tx/client.h>
#include <usb_session/capability.h>
#include <usb_session/usb_session.h>

namespace Usb {
	class  Session_client;
	class  Interface_client;
}


class Usb::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Packet_stream_tx::Client<Tx> _tx;

	public:

		/**
		 * Constructor
		 *
		 * \param session          session capability
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 */
		Session_client(Session_capability       session,
		               Genode::Range_allocator &tx_buffer_alloc,
		               Genode::Region_map      &rm,
		               Genode::Signal_context_capability state_change)
		:
			Genode::Rpc_client<Session>(session),
			_tx(call<Rpc_tx_cap>(), rm, tx_buffer_alloc)
		{
			if (state_change.valid())
				sigh_state_change(state_change);
		}

		/***************************
		 ** USB session interface **
		 ***************************/

		bool plugged()       override { return call<Rpc_plugged>(); }
		Tx *tx_channel()     override { return &_tx; }
		Tx::Source *source() override { return _tx.source(); }

		void sigh_state_change(Genode::Signal_context_capability sigh) override {
			call<Rpc_sigh_state_change>(sigh); }

		void config_descriptor(Device_descriptor *device_descr,
		                           Config_descriptor *config_descr) override
		{
			call<Rpc_config_descr>(device_descr, config_descr);
		}

		unsigned alt_settings(unsigned index) override
		{
			return call<Rpc_alt_settings>(index);
		}

		void interface_descriptor(unsigned index, unsigned alt_setting,
		                              Interface_descriptor *interface_descr)  override
		{
			call<Rpc_iface_descr>(index, alt_setting, interface_descr);
		}

		void endpoint_descriptor(unsigned              interface_num,
		                             unsigned              alt_setting,
		                             unsigned              endpoint_num,
		                             Endpoint_descriptor  *endpoint_descr) override
		{
			call<Rpc_ep_descr>(interface_num, alt_setting, endpoint_num, endpoint_descr);
		}

		void claim_interface(unsigned interface_num) override
		{
			call<Rpc_claim_interface>(interface_num);
		}

		void release_interface(unsigned interface_num) override
		{
			call<Rpc_release_interface>(interface_num);
		}
};

#endif /* _INCLUDE__USB_SESSION__CLIENT_H_ */
