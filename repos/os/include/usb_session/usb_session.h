/**
 * \brief  USB session for USB clients (mainly device drivers)
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB_SESSION__USB_SESSION_H_
#define _INCLUDE__USB_SESSION__USB_SESSION_H_

#include <base/rpc_args.h>
#include <os/packet_stream.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <rom_session/capability.h>
#include <session/session.h>
#include <usb_session/capability.h>

namespace Usb {
	using namespace Genode;

	struct Tagged_packet;
	struct Interface_session;
	struct Device_session;
	class  Session;
}


struct Usb::Tagged_packet : Genode::Packet_descriptor
{

	/*
	 * At least on ARM the minimal alignment for distinct
	 * DMA-capable USB URBs shall meet a maximum cache-line
	 * size of 128 bytes
	 */
	enum Alignment { PACKET_ALIGNMENT = 7 };

	/**
	 * Payload location within the packet stream
	 */
	struct Payload
	{
		off_t  offset;
		size_t bytes;
	};

	struct Tag {
		unsigned long value;
	} tag { ~0UL };

	enum Return_value {
		UNHANDLED, NO_DEVICE, INVALID, TIMEOUT, HALT, OK
	} return_value { UNHANDLED };


	Tagged_packet(off_t offset = 0, size_t size = 0)
	: Genode::Packet_descriptor(offset, size) {}

	Tagged_packet(Payload p, Tag tag)
	:
		Genode::Packet_descriptor(p.offset, p.bytes),
		tag(tag) {}
};


struct Usb::Interface_session : Interface
{
	struct Packet_descriptor : Tagged_packet
	{
		enum Type {
			BULK, IRQ, ISOC, FLUSH
		} type { FLUSH };

		uint8_t index { 0 };
		size_t  payload_return_size { 0 };

		using Tagged_packet::Tagged_packet;
	};

	enum { TX_QUEUE_SIZE = 64 };

	using Tx_policy = Packet_stream_policy<Packet_descriptor,
	                                       TX_QUEUE_SIZE, TX_QUEUE_SIZE, char>;
	using Tx = Packet_stream_tx::Channel<Tx_policy>;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_tx_cap, Capability<Tx>, tx_cap);
	GENODE_RPC_INTERFACE(Rpc_tx_cap);
};


struct Usb::Device_session : Interface
{
	struct Packet_descriptor : Tagged_packet
	{
		enum Request : uint8_t {
			GET_STATUS        = 0x00,
			CLEAR_FEATURE     = 0x01,
			SET_FEATURE       = 0x03,
			SET_ADDRESS       = 0x05,
			GET_DESCRIPTOR    = 0x06,
			SET_DESCRIPTOR    = 0x07,
			GET_CONFIGURATION = 0x08,
			SET_CONFIGURATION = 0x09,
			GET_INTERFACE     = 0x0a,
			SET_INTERFACE     = 0x0b,
			SYNCH_FRAME       = 0x0c,
			SET_SEL           = 0x30,
			SET_ISOCH_DELAY   = 0x31,
		};

		uint8_t request { GET_STATUS };

		enum Recipient { DEVICE, IFACE, ENDP, OTHER };
		enum Type      { STANDARD, CLASS, VENDOR, RESERVED };
		enum Direction { OUT, IN };

		struct Request_type : Register<8>
		{
			struct R : Bitfield<0, 5> { };
			struct T : Bitfield<5, 2> { };
			struct D : Bitfield<7, 1> { };

			static access_t value(Recipient r, Type t, Direction d)
			{
				access_t ret = 0;
				R::set(ret, r);
				T::set(ret, t);
				D::set(ret, d);
				return ret;
			}
		};

		Request_type::access_t request_type { 0 };

		uint16_t value { 0 };
		uint16_t index { 0 };

		size_t payload_return_size { 0 };
		size_t timeout             { 0 };

		using Tagged_packet::Tagged_packet;
	};

	enum { TX_QUEUE_SIZE = 8, TX_BUFFER_SIZE = 4096 };

	using Tx_policy = Packet_stream_policy<Packet_descriptor,
	                                       TX_QUEUE_SIZE, TX_QUEUE_SIZE, char>;
	using Tx = Packet_stream_tx::Channel<Tx_policy>;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC_THROW(Rpc_acquire_interface, Interface_capability,
	                 acquire_interface,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 uint8_t, size_t);
	GENODE_RPC(Rpc_release_interface, void, release_interface,
	           Interface_capability);
	GENODE_RPC(Rpc_tx_cap, Capability<Tx>, tx_cap);
	GENODE_RPC_INTERFACE(Rpc_acquire_interface, Rpc_release_interface,
	                     Rpc_tx_cap);
};


struct Usb::Session : public Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Usb"; }

	static constexpr unsigned CAP_QUOTA = 8;
	static constexpr unsigned RAM_QUOTA = 512 * 1024;

	virtual ~Session() {}

	using Device_name = String<64>;

	/**
	 * Request ROM session containing information about available devices.
	 *
	 * \return  capability to ROM dataspace
	 */
	virtual Rom_session_capability devices_rom() = 0;

	/**
	 * Acquire device known by unique 'name'
	 */
	virtual Device_capability acquire_device(Device_name const &name) = 0;

	/**
	 * Acquire the first resp. single device of this session
	 */
	virtual Device_capability acquire_single_device() = 0;

	/**
	 * Release all resources regarding the given 'device' session
	 */
	virtual void release_device(Device_capability device) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_devices_rom, Rom_session_capability, devices_rom);
	GENODE_RPC_THROW(Rpc_acquire_device, Device_capability, acquire_device,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 Device_name const &);
	GENODE_RPC_THROW(Rpc_acquire_single_device, Device_capability,
	                 acquire_single_device,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps));
	GENODE_RPC(Rpc_release_device, void, release_device, Device_capability);
	GENODE_RPC_INTERFACE(Rpc_devices_rom, Rpc_acquire_device,
	                     Rpc_acquire_single_device, Rpc_release_device);
};

#endif /* _INCLUDE__USB_SESSION__USB_SESSION_H_ */
