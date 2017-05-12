/*
 * \brief  NIC driver for Linux TUN/TAP device
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2011-08-08
 *
 * Configuration options are:
 *
 * - TAP device to connect to (default is tap0)
 * - MAC address (default is 02-00-00-00-00-01)
 *
 * These can be set in the config section as follows:
 *  <config>
 *  	<nic mac="12:23:34:45:56:67" tap="tap1"/>
 *  </config>
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
/*
 * Needs to be included first because otherwise
 * util/xml_node.h will not pick up the ascii_to
 * overload.
 */
#include <nic/xml_node.h>

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/thread.h>
#include <base/log.h>
#include <nic/root.h>

/* Linux */
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

namespace Server {
	using namespace Genode;

	struct Main;
}


class Linux_session_component : public Nic::Session_component
{
	private:

		struct Rx_signal_thread : Genode::Thread
		{
			int                               fd;
			Genode::Signal_context_capability sigh;

			Rx_signal_thread(Genode::Env &env, int fd, Genode::Signal_context_capability sigh)
			: Genode::Thread(env, "rx_signal", 0x1000), fd(fd), sigh(sigh) { }

			void entry()
			{
				while (true) {
					/* wait for packet arrival on fd */
					int    ret;
					fd_set rfds;

					FD_ZERO(&rfds);
					FD_SET(fd, &rfds);
					do { ret = select(fd + 1, &rfds, 0, 0, 0); } while (ret < 0);

					/* signal incoming packet */
					Genode::Signal_transmitter(sigh).submit();
				}
			}
		};

		Genode::Attached_rom_dataspace _config_rom;

		Nic::Mac_address _mac_addr;
		int              _tap_fd;
		Rx_signal_thread _rx_thread;

		int _setup_tap_fd()
		{
			/* open TAP device */
			int ret;
			struct ifreq ifr;

			int fd = open("/dev/net/tun", O_RDWR);
			if (fd < 0) {
				Genode::error("could not open /dev/net/tun: no virtual network emulation");
				/* this error is fatal */
				throw Genode::Exception();
			}

			/* set fd to non-blocking */
			if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
				Genode::error("could not set /dev/net/tun to non-blocking");
				throw Genode::Exception();
			}

			Genode::memset(&ifr, 0, sizeof(ifr));
			ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

			/* get tap device from config */
			try {
				Genode::Xml_node nic_node = _config_rom.xml().sub_node("nic");
				nic_node.attribute("tap").value(ifr.ifr_name, sizeof(ifr.ifr_name));
				Genode::log("using tap device \"", Genode::Cstring(ifr.ifr_name), "\"");
			} catch (...) {
				/* use tap0 if no config has been provided */
				Genode::strncpy(ifr.ifr_name, "tap0", sizeof(ifr.ifr_name));
				Genode::log("no config provided, using tap0");
			}

			ret = ioctl(fd, TUNSETIFF, (void *) &ifr);
			if (ret != 0) {
				Genode::error("could not configure /dev/net/tun: no virtual network emulation");
				close(fd);
				/* this error is fatal */
				throw Genode::Exception();
			}

			return fd;
		}

		bool _send()
		{
			using namespace Genode;

			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.size()) {
				warning("invalid tx packet");
				return true;
			}

			int ret;

			/* non-blocking-write packet to TAP */
			do {
				ret = write(_tap_fd, _tx.sink()->packet_content(packet), packet.size());
				/* drop packet if write would block */
				if (ret < 0 && errno == EAGAIN)
					continue;

				if (ret < 0) Genode::error("write: errno=", errno);
			} while (ret < 0);

			_tx.sink()->acknowledge_packet(packet);

			return true;
		}

		bool _receive()
		{
			unsigned const max_size = Nic::Packet_allocator::DEFAULT_PACKET_SIZE;

			if (!_rx.source()->ready_to_submit())
				return false;

			Nic::Packet_descriptor p;
			try {
				p = _rx.source()->alloc_packet(max_size);
			} catch (Session::Rx::Source::Packet_alloc_failed) { return false; }

			int size = read(_tap_fd, _rx.source()->packet_content(p), max_size);
			if (size <= 0) {
				_rx.source()->release_packet(p);
				return false;
			}

			/* adjust packet size */
			Nic::Packet_descriptor p_adjust(p.offset(), size);
			_rx.source()->submit_packet(p_adjust);

			return true;
		}

	protected:

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			while (_send()) ;
			while (_receive()) ;
		}

	public:

		Linux_session_component(Genode::size_t const tx_buf_size,
		                        Genode::size_t const rx_buf_size,
		                        Genode::Allocator   &rx_block_md_alloc,
		                        Server::Env         &env)
		:
			Session_component(tx_buf_size, rx_buf_size, rx_block_md_alloc, env),
			_config_rom(env, "config"),
			_tap_fd(_setup_tap_fd()), _rx_thread(env, _tap_fd, _packet_stream_dispatcher)
		{
			/* try using configured MAC address */
			try {
				Genode::Xml_node nic_config = _config_rom.xml().sub_node("nic");
				nic_config.attribute("mac").value(&_mac_addr);
				Genode::log("Using configured MAC address ", _mac_addr);
			} catch (...) {
				/* fall back to fake MAC address (unicast, locally managed) */
				_mac_addr.addr[0] = 0x02;
				_mac_addr.addr[1] = 0x00;
				_mac_addr.addr[2] = 0x00;
				_mac_addr.addr[3] = 0x00;
				_mac_addr.addr[4] = 0x00;
				_mac_addr.addr[5] = 0x01;
			}

			_rx_thread.start();
		}

	bool link_state() override              { return true; }
	Nic::Mac_address mac_address() override { return _mac_addr; }
};


struct Server::Main
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Nic::Root<Linux_session_component> nic_root { _env, _heap };

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(nic_root));
	}
};

void Component::construct(Genode::Env &env) { static Server::Main main(env); }
