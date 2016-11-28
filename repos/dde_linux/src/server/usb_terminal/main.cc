/*
 * \brief  PL2303-USB-UART driver that exposes a terminal session
 * \author Sebastian Sumpf
 * \date   2014-12-17
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/heap.h>
#include <os/attached_ram_dataspace.h>
#include <os/ring_buffer.h>
#include <os/server.h>
#include <root/component.h>
#include <terminal_session/terminal_session.h>
#include <usb/usb.h>


namespace Terminal {
	class Main;
	class Root_component;
	class Session_component;
	using namespace Genode;
}


namespace Usb {
	struct Pl2303_driver;
	using namespace Genode;
}


struct pl2303_config
{
	Genode::uint32_t baud      = 115200;
	Genode::uint8_t  stop_bits = 0;
	Genode::uint8_t  parity    = 0;
	Genode::uint8_t  data_bits = 8;
} __attribute__((packed));


struct Usb::Pl2303_driver : Completion
{
	enum { VENDOR = 0x67b, PRODUCT = 0x2303 /* Prolific 2303 seriral port */ };

	enum {
		PACKET_BUFFER   = 2,    /* number of read packets */
		RING_SIZE       = 4096, /* size of ring buffer    */
		MAX_PACKET_SIZE = 256   /* max packet size for bulk request */
	};

	/* Pl2303 endpoints */
	enum Endpoints { STATUS = 0,  OUT = 1, IN = 2 };

	typedef Genode::Ring_buffer<char, 4096, Genode::Ring_buffer_unsynchronized> Ring_buffer;

	Ring_buffer                              ring_buffer;
	Server::Entrypoint                      &ep;
	Server::Signal_rpc_member<Pl2303_driver> dispatcher{ ep, *this, &Pl2303_driver::state_change };
	Genode::Allocator_avl                    alloc;
	Usb::Connection                          connection{ &alloc, "usb_serial", 512 * 1024, dispatcher };
	Usb::Device                              device;
	Signal_context_capability                connected_sigh;
	Signal_context_capability                read_sigh;


	Pl2303_driver(Server::Entrypoint &ep)
	: ep(ep), alloc(Genode::env()->heap()),
	  device(Genode::env()->heap(), connection, ep)
	{ }


	/* send control message for to read/write from/to PL2303 endpoint */
	void pl2303_magic(Usb::Interface &iface, uint16_t value, uint16_t index, bool read)
	{
		Usb::Packet_descriptor p = iface.alloc(0);

		uint8_t request_type = read ? Usb::ENDPOINT_IN : Usb::ENDPOINT_OUT
		                        | Usb::TYPE_VENDOR
		                        | Usb::RECIPIENT_DEVICE;

		iface.control_transfer(p, request_type, 1, value, index,100);
		iface.release(p);
	}

	void pl2303_magic_read(Usb::Interface &iface, uint16_t value, uint16_t index) {
		pl2303_magic(iface, value, index, true); }

	void pl2303_magic_write(Usb::Interface &iface, uint16_t value, uint16_t index) {
		pl2303_magic(iface, value, index, false); }

	void bulk_packet(Packet_descriptor &p)
	{
		Interface iface = device.interface(0);

		/* error or write packet */
		if (!p.succeded || !p.read_transfer()) {
			iface.release(p);
			return;
		}

		/* buffer data */
		bool send_sigh = ring_buffer.empty() && p.transfer.actual_size;
		char *data     = (char *)iface.content(p);

		try {
			for (int i = 0; i < p.transfer.actual_size; i++)
				ring_buffer.add(data[i]);
		} catch (Ring_buffer::Overflow) {
			Genode::warning("Pl2303 buffer overflow");
		}

		/* submit back to device (async) */
		iface.submit(p);

		/* notify client */
		if (send_sigh && read_sigh.valid())
			Signal_transmitter(read_sigh).submit();
	}

	void complete(Packet_descriptor &p)
	{
		switch (p.type) {
			case Packet_descriptor::BULK: bulk_packet(p); break;
			default: break;
		}
	}

	size_t write(void *dst, size_t num_bytes)
	{
		num_bytes = min(num_bytes, MAX_PACKET_SIZE);

		Interface         &iface = device.interface(0);
		Endpoint          &ep    = iface.endpoint(OUT);
		Packet_descriptor  p     = iface.alloc(num_bytes);

		memcpy(iface.content(p), dst, num_bytes);
		iface.bulk_transfer(p, ep, false, this);

		return num_bytes;
	}

	/**
	 * Init 2303 controller
	 */
	void init()
	{
		/* read device configuration */
		device.update_config();

		enum { BUF = 128 };
		char buffer[BUF];

		Genode::log("PL2303 controller: ready");
		Genode::log("Manufacturer     : ", Cstring(device.manufactorer_string.to_char(buffer, BUF)));
		Genode::log("Product          : ", Cstring(device.product_string.to_char(buffer, BUF)));

		Interface &iface = device.interface(0);
		iface.claim();

		/* undocumented magic, taken from Linux and GRUB */
		pl2303_magic_read (iface, 0x8484, 0x0000);
		pl2303_magic_write(iface, 0x0404, 0x0000);
		pl2303_magic_read (iface, 0x8484, 0x0000);
		pl2303_magic_read (iface, 0x8383, 0x0000);
		pl2303_magic_read (iface, 0x8484, 0x0000);
		pl2303_magic_write(iface, 0x0404, 0x0001);
		pl2303_magic_read (iface, 0x8484, 0x0000);
		pl2303_magic_read (iface, 0x8383, 0x0000);
		pl2303_magic_write(iface, 0x0000, 0x0001);
		pl2303_magic_write(iface, 0x0001, 0x0000);
		pl2303_magic_write(iface, 0x0002, 0x0044);
		pl2303_magic_write(iface, 0x0008, 0x0000);
		pl2303_magic_write(iface, 0x0009, 0x0000);

		Usb::Packet_descriptor p = iface.alloc(sizeof(pl2303_config));
		iface.control_transfer(p, 0xa1, 0x21, 0, 0, 100);

		/* set baud rate to 115200 */
		pl2303_config *cfg = (pl2303_config *)iface.content(p);

		pl2303_config cfg_new;
		*cfg = cfg_new;

		iface.control_transfer(p, 0x21, 0x20, 0, 0, 100);
		iface.release(p);

		/* finish it */
		pl2303_magic_write(iface, 0x0, 0x0);

		/* submit read transfers (async) */
		Usb::Endpoint &ep = iface.endpoint(IN);
		for (int i = 0; i < PACKET_BUFFER; i++) {
			p = iface.alloc(ep.max_packet_size);
			iface.bulk_transfer(p, ep, false, this);
		}

		/* send signal to terminal client */
		if (connected_sigh.valid())
			Signal_transmitter(connected_sigh).submit();
	}

	/**
	 * Called from USB driver when the desired device is active
	 */
	void state_change(unsigned)
	{
		if (connection.plugged())
			init();
	}

	/**
	 * Return true if data is available
	 */
	bool avail() const { return !ring_buffer.empty(); }

	/**
	 * Obtain character
	 */
	char get() { return ring_buffer.get(); }
};


class Terminal::Session_component : public Rpc_object<Session, Session_component>
{
	private:

		Attached_ram_dataspace  _io_buffer;
		Usb::Pl2303_driver     &_driver;

	public:

		Session_component(size_t io_buffer_size, Usb::Pl2303_driver &driver)
		: _io_buffer(env()->ram_session(), io_buffer_size), _driver(driver)
		{ }

		void read_avail_sigh(Signal_context_capability sigh)
		{
			_driver.read_sigh = sigh;
		}

		void connected_sigh(Signal_context_capability sigh)
		{
			_driver.connected_sigh = sigh;
		}

		Size size()  { return Size(0, 0); }
		bool avail() { return _driver.avail(); }

		size_t _read(size_t dst_len)
		{
			size_t num_bytes = 0;
			char  *data      = _io_buffer.local_addr<char>();

			for (;num_bytes < dst_len && _driver.avail(); num_bytes++)
				data[num_bytes] = _driver.get();

			return num_bytes;
		}

		void _write(size_t num_bytes)
		{
			char *dst = _io_buffer.local_addr<char>();
			while (num_bytes) {
				size_t size = _driver.write(dst, num_bytes);
				num_bytes -= size;
				dst       += size;
			}
		}

		Dataspace_capability _dataspace() { return _io_buffer.cap(); }

		size_t read(void *buf, size_t) { return 0; }
		size_t write(void const *buf, size_t) { return 0; }
};


class Terminal::Root_component : public Genode::Root_component<Session_component, Single_client>
{
	private:

		Usb::Pl2303_driver _driver;

	protected:

		Session_component *_create_session(char const *args)
		{
			size_t const io_buffer_size = 4096;
			return new (md_alloc()) Session_component(io_buffer_size, _driver);
		}

	public:

		Root_component(Server::Entrypoint &ep, Genode::Allocator &md_alloc)
		: Genode::Root_component<Session_component, Genode::Single_client>(&ep.rpc_ep(), &md_alloc),
			_driver(ep)
		{ }
};


struct Terminal::Main
{
	Server::Entrypoint &ep;
	Sliced_heap         heap = { env()->ram_session(), env()->rm_session() };
	Root_component      terminal_root = { ep, heap };

	Main(Server::Entrypoint &ep) :  ep(ep)
	{
		env()->parent()->announce(ep.manage(terminal_root));
	}
};


namespace Server {
	char const *name()       { return "usb_terminal_ep"; };
	size_t      stack_size() { return 16*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Terminal::Main server(ep);
	}
}
