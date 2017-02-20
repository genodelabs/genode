/*
 * \brief  PL2303-USB-UART driver that exposes a terminal session
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2014-12-17
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/component.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <os/ring_buffer.h>
#include <os/static_root.h>
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

	Env                   &_env;
	Heap                  &_heap;
	Ring_buffer            _ring_buffer;
	Genode::Allocator_avl  _alloc { &_heap };

	Genode::Signal_handler<Pl2303_driver> _state_handler {
		_env.ep(), *this, &Pl2303_driver::_handle_state_change };

	Usb::Connection _connection { &_alloc, "usb_serial", 512 * 1024, _state_handler };
	Usb::Device     _device     { &_heap, _connection, _env.ep() };

	Signal_context_capability _connected_sigh;
	Signal_context_capability _read_avail_sigh;

	/**
	 * Init 2303 controller
	 */
	void _init()
	{
		/* read device configuration */
		_device.update_config();

		enum { BUF = 128 };
		char buffer[BUF];

		Genode::log("PL2303 controller: ready");
		Genode::log("Manufacturer     : ", Cstring(_device.manufactorer_string.to_char(buffer, BUF)));
		Genode::log("Product          : ", Cstring(_device.product_string.to_char(buffer, BUF)));

		Interface &iface = _device.interface(0);
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
		if (_connected_sigh.valid())
			Signal_transmitter(_connected_sigh).submit();
	}

	void _handle_state_change()
	{
		if (_connection.plugged())
			_init();
	}



	Pl2303_driver(Genode::Env &env, Heap &heap)
	: _env(env), _heap(heap) { }

	void read_avail_sigh(Signal_context_capability sigh)
	{
		_read_avail_sigh = sigh;
	}

	void connected_sigh(Signal_context_capability sigh)
	{
		_connected_sigh = sigh;
	}


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
		Interface iface = _device.interface(0);

		/* error or write packet */
		if (!p.succeded || !p.read_transfer()) {
			iface.release(p);
			return;
		}

		/* buffer data */
		bool const notify_sigh = _ring_buffer.empty() && p.transfer.actual_size;
		char *data             = (char *)iface.content(p);

		try {
			for (int i = 0; i < p.transfer.actual_size; i++)
				_ring_buffer.add(data[i]);
		} catch (Ring_buffer::Overflow) {
			Genode::warning("Pl2303 buffer overflow");
		}

		/* submit back to device (async) */
		iface.submit(p);

		/* notify client */
		if (notify_sigh && _read_avail_sigh.valid())
			Signal_transmitter(_read_avail_sigh).submit();
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

		Interface         &iface = _device.interface(0);
		Endpoint          &ep    = iface.endpoint(OUT);
		Packet_descriptor  p     = iface.alloc(num_bytes);

		memcpy(iface.content(p), dst, num_bytes);
		iface.bulk_transfer(p, ep, false, this);

		return num_bytes;
	}

	/**
	 * Return true if data is available
	 */
	bool avail() const { return !_ring_buffer.empty(); }

	/**
	 * Obtain character
	 */
	char get() { return _ring_buffer.get(); }
};


class Terminal::Session_component : public Rpc_object<Session, Session_component>
{
	private:

		Attached_ram_dataspace  _io_buffer;
		Usb::Pl2303_driver     &_driver;

	public:

		Session_component(Genode::Env &env, size_t io_buffer_size,
		                  Usb::Pl2303_driver &driver)
		:
			_io_buffer(env.ram(), env.rm(), io_buffer_size),
			_driver(driver)
		{ }

		void read_avail_sigh(Signal_context_capability sigh)
		{
			_driver.read_avail_sigh(sigh);
		}

		void connected_sigh(Signal_context_capability sigh)
		{
			_driver.connected_sigh(sigh);
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

		size_t _write(size_t num_bytes)
		{
			char *dst = _io_buffer.local_addr<char>();
			size_t written_bytes;
			for (written_bytes = 0; written_bytes < num_bytes; ) {
				written_bytes += _driver.write(dst + written_bytes,
				                               num_bytes - written_bytes);
			}
			return written_bytes;
		}

		Dataspace_capability _dataspace() { return _io_buffer.cap(); }

		size_t read(void *buf, size_t) { return 0; }
		size_t write(void const *buf, size_t) { return 0; }
};


struct Terminal::Main
{
	Env                  &_env;
	Heap                  _heap    { _env.ram(), _env.rm() };
	Usb::Pl2303_driver    _driver  { _env, _heap };
	Session_component     _session { _env, 4096, _driver };
	Static_root<Session>  _root    { _env.ep().manage(_session) };

	Main(Genode::Env &env) : _env(env)
	{
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Terminal::Main inst(env); }
