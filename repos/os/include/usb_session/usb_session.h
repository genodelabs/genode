/**
 * \brief  USB session for raw device connection
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB_SESSION__USB_SESSION_H_
#define _INCLUDE__USB_SESSION__USB_SESSION_H_

#include <os/packet_stream.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <session/session.h>
#include <usb/types.h>


namespace Usb {

	using namespace Genode;
	class Session;
	struct Packet_descriptor;
	struct Completion;
}


/**
 * USB packet type
 */
struct Usb::Packet_descriptor : Genode::Packet_descriptor
{
	enum Type { STRING, CTRL, BULK, IRQ, ISOC, ALT_SETTING, CONFIG, RELEASE_IF };
	enum Iso  { MAX_PACKETS = 32 };

	/* use the polling interval stated in the endpoint descriptor */
	enum { DEFAULT_POLLING_INTERVAL = -1 };

	Type        type       { STRING };
	bool        succeded   { false };
	Completion *completion { nullptr };

	union
	{
		struct
		{
			uint8_t  index;
			unsigned length;
		} string;

		struct
		{
			uint8_t  request;
			uint8_t  request_type;
			uint16_t value;
			uint16_t index;
			int      actual_size; /* returned */
			int      timeout;
		} control;

		struct
		{
			uint8_t ep;
			int     actual_size; /* returned */
			int     polling_interval; /* for interrupt transfers */
			int     number_of_packets;
			size_t  packet_size[MAX_PACKETS];
		} transfer;

		struct
		{
			uint8_t number;
			uint8_t alt_setting;
		} interface;

		struct
		{
			uint8_t number;
		};
	};

	enum Error { NO_ERROR, STALL_ERROR, SUBMIT_ERROR };

	Error error = NO_ERROR;

	/**
	 * Return true if packet is a read transfer
	 */
	bool read_transfer() { return transfer.ep & ENDPOINT_IN; }

	/**
	 * Return true if packet is a read transfer
	 *
	 * \noapi
	 * \deprecated  use 'read_transfer' instead
	 */
	bool is_read_transfer() { return read_transfer(); }

	Packet_descriptor(off_t offset = 0, size_t size = 0)
	: Genode::Packet_descriptor(offset, size) { }

	Packet_descriptor(Genode::Packet_descriptor p, Type type, Completion *completion = nullptr)
	: Genode::Packet_descriptor(p.offset(), p.size()), type(type), completion(completion) { }
};


/**
 * Completion for asynchronous communication
 */
struct Usb::Completion : Genode::Interface
{
	virtual void complete(Usb::Packet_descriptor &p) = 0;
};


struct Usb::Session : public Genode::Session
{
	/****************
	 ** Exceptions **
	 ****************/

	class Device_not_found          : public Exception { };
	class Interface_not_found       : public Exception { };
	class Interface_already_claimed : public Exception { };
	class Interface_not_claimed     : public Exception { };
	class Invalid_endpoint          : public Exception { };


	/*******************
	 ** Packet stream **
	 *******************/

	enum { TX_QUEUE_SIZE = 64 };

	typedef Packet_stream_policy<Usb::Packet_descriptor,
	                             TX_QUEUE_SIZE, TX_QUEUE_SIZE,
	                             char> Tx_policy;

	typedef Packet_stream_tx::Channel<Tx_policy> Tx;

	/**
	 * Request packet-transmission channel
	 */
	virtual Tx *tx_channel() { return 0; }

	/**
	 * Request client-side packet-stream interface of tx channel
	 */
	virtual Tx::Source *source() { return 0; }


	/***********************
	 ** Session interface **
	 ***********************/

	/**
	 * \noapi
	 */
	static const char *service_name() { return "Usb"; }

	enum { CAP_QUOTA = 5 };

	/**
	 * Send from the server to the client upon device state change
	 */
	virtual void sigh_state_change(Signal_context_capability sigh) = 0;

	/**
	 * Is the device present
	 */
	virtual bool plugged() = 0;

	/**
	 * Retrieve device and current configurations despcriptors
	 */
	virtual void config_descriptor(Device_descriptor *device_descr,
	                               Config_descriptor *config_descr) = 0;

	/**
	 * Return number of alt settings for iterface
	 */
	virtual unsigned alt_settings(unsigned index) = 0;

	/**
	 * Return interface descriptor for interface index/alternate setting tuple
	 */
	virtual void interface_descriptor(unsigned index, unsigned alt_setting,
	                                  Interface_descriptor *interface_descr) = 0;

	/**
	 * Return endpoint for interface index/alternate setting tuple
	 */
	virtual void endpoint_descriptor(unsigned              interface_num,
	                                 unsigned              alt_setting,
	                                 unsigned              endpoint_num,
	                                 Endpoint_descriptor  *endpoint_descr) = 0;

	/**
	 * Claim an interface number
	 */
	virtual void claim_interface(unsigned interface_num) = 0;

	/**
	 * Release an interface number
	 */
	virtual void release_interface(unsigned interface_num) = 0;

	GENODE_RPC(Rpc_plugged, bool, plugged);
	GENODE_RPC(Rpc_sigh_state_change, void, sigh_state_change, Signal_context_capability);
	GENODE_RPC(Rpc_tx_cap, Capability<Tx>, _tx_cap);
	GENODE_RPC_THROW(Rpc_config_descr, void, config_descriptor, GENODE_TYPE_LIST(Device_not_found),
	                 Device_descriptor *, Config_descriptor *);
	GENODE_RPC(Rpc_alt_settings, unsigned, alt_settings, unsigned);
	GENODE_RPC_THROW(Rpc_iface_descr, void, interface_descriptor, GENODE_TYPE_LIST(Device_not_found,
	                 Interface_not_found), unsigned, unsigned, Interface_descriptor *);
	GENODE_RPC_THROW(Rpc_ep_descr, void, endpoint_descriptor, GENODE_TYPE_LIST(Device_not_found,
	                 Interface_not_found), unsigned, unsigned, unsigned, Endpoint_descriptor *);
	GENODE_RPC_THROW(Rpc_claim_interface, void, claim_interface, GENODE_TYPE_LIST(Interface_not_found,
	                 Interface_already_claimed), unsigned);
	GENODE_RPC_THROW(Rpc_release_interface, void, release_interface, GENODE_TYPE_LIST(Interface_not_found),
	                 unsigned);
	GENODE_RPC_INTERFACE(Rpc_plugged, Rpc_sigh_state_change, Rpc_tx_cap, Rpc_config_descr,
	                     Rpc_iface_descr, Rpc_ep_descr, Rpc_alt_settings, Rpc_claim_interface,
	                     Rpc_release_interface);
};

#endif /* _INCLUDE__USB_SESSION__USB_SESSION_H_ */
