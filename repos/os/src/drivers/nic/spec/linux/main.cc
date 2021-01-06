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
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/thread.h>
#include <base/log.h>
#include <base/blockade.h>
#include <nic/root.h>

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>
#include <drivers/nic/mode.h>

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


static Net::Mac_address default_mac_address()
{
	/* fall back to fake MAC address (unicast, locally managed) */
	Nic::Mac_address mac_addr { };
	mac_addr.addr[0] = 0x02;
	mac_addr.addr[1] = 0x00;
	mac_addr.addr[2] = 0x00;
	mac_addr.addr[3] = 0x00;
	mac_addr.addr[4] = 0x00;
	mac_addr.addr[5] = 0x01;
	return mac_addr;
}


class Linux_session_component : public Nic::Session_component
{
	private:

		struct Rx_signal_thread : Genode::Thread
		{
			int                               fd;
			Genode::Signal_context_capability sigh;
			Genode::Blockade                  blockade { };

			Rx_signal_thread(Genode::Env &env, int fd, Genode::Signal_context_capability sigh)
			: Genode::Thread(env, "rx_signal", 0x1000), fd(fd), sigh(sigh) { }

			void entry() override
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

					blockade.block();
				}
			}
		};

		Genode::Attached_rom_dataspace _config_rom;

		Nic::Mac_address _mac_addr { };
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
				nic_node.attribute("tap").with_raw_value([&] (char const *ptr, size_t len) {
					len = Genode::min(len, sizeof(ifr.ifr_name) - 1);
					Genode::memcpy(ifr.ifr_name, ptr, len);
					ifr.ifr_name[len] = 0;
				});
				Genode::log("using tap device \"", Genode::Cstring(ifr.ifr_name), "\"");
			} catch (...) {
				/* use tap0 if no config has been provided */
				Genode::copy_cstring(ifr.ifr_name, "tap0", sizeof(ifr.ifr_name));
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
			if (!packet.size() || !_tx.sink()->packet_valid(packet)) {
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

		enum class Receive_result {
			NO_PACKET, READ_ERROR, SUBMITTED, ALLOC_FAILED, SUBMIT_QUEUE_FULL };

		Receive_result _receive()
		{
			unsigned const max_size = Nic::Packet_allocator::DEFAULT_PACKET_SIZE;

			if (!_rx.source()->ready_to_submit())
				return Receive_result::SUBMIT_QUEUE_FULL;

			Nic::Packet_descriptor p;
			try {
				p = _rx.source()->alloc_packet(max_size);
			} catch (Session::Rx::Source::Packet_alloc_failed) { return Receive_result::ALLOC_FAILED; }

			int size = read(_tap_fd, _rx.source()->packet_content(p), max_size);
			if (size <= 0) {
				_rx.source()->release_packet(p);
				return errno == EAGAIN ? Receive_result::NO_PACKET
				                       : Receive_result::READ_ERROR;
			}

			/* adjust packet size */
			Nic::Packet_descriptor p_adjust(p.offset(), size);
			_rx.source()->submit_packet(p_adjust);

			return Receive_result::SUBMITTED;
		}

	protected:

		bool _handle_incoming_packets()
		{
			while (true) {
				switch (_receive()) {
				case Receive_result::NO_PACKET:         return true;
				case Receive_result::READ_ERROR:        return true;
				case Receive_result::SUBMITTED:         continue;
				case Receive_result::ALLOC_FAILED:      return false;
				case Receive_result::SUBMIT_QUEUE_FULL: return false;
				}
			}
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail()) {
				_rx.source()->release_packet(_rx.source()->get_acked_packet());
			}

			while (_send()) ;

			if (_handle_incoming_packets())
				_rx_thread.blockade.wakeup();
		}

	public:

		Linux_session_component(Genode::size_t const tx_buf_size,
		                        Genode::size_t const rx_buf_size,
		                        Genode::Allocator   &rx_block_md_alloc,
		                        Server::Env         &env)
		:
			Session_component(tx_buf_size, rx_buf_size, Genode::CACHED, rx_block_md_alloc, env),
			_config_rom(env, "config"),
			_tap_fd(_setup_tap_fd()),
			_rx_thread(env, _tap_fd, _packet_stream_dispatcher)
		{
			_mac_addr = default_mac_address();

			/* try using configured MAC address */
			try {
				Genode::Xml_node nic_config = _config_rom.xml().sub_node("nic");
				_mac_addr = nic_config.attribute_value("mac", _mac_addr);
				Genode::log("Using configured MAC address ", _mac_addr);
			} catch (...) { }

			_rx_thread.start();
		}

	bool link_state() override              { return true; }
	Nic::Mac_address mac_address() override { return _mac_addr; }
};


class Uplink_client : public Genode::Uplink_client_base
{
	private:

		struct Rx_signal_thread : Genode::Thread
		{
			int                               fd;
			Genode::Signal_context_capability sigh;
			Genode::Blockade                  blockade { };

			Rx_signal_thread(Genode::Env &env, int fd, Genode::Signal_context_capability sigh)
			: Genode::Thread(env, "rx_signal", 0x1000), fd(fd), sigh(sigh) { }

			void entry() override
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

					blockade.block();
				}
			}
		};

		int                                    _tap_fd;
		Genode::Signal_handler<Uplink_client>  _rx_handler { _env.ep(), *this, &Uplink_client::_handle_rx };
		Rx_signal_thread                       _rx_thread  { _env, _tap_fd, _rx_handler };

		static int _init_tap_fd(Genode::Xml_node const &config)
		{
			/* open TAP device */
			int ret;
			struct ifreq ifr;

			int fd = open("/dev/net/tun", O_RDWR);
			if (fd < 0) {
				Genode::error("could not open /dev/net/tun: no virtual network emulation");
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
				Genode::Xml_node nic_node = config.sub_node("nic");
				nic_node.attribute("tap").with_raw_value([&] (char const *ptr, size_t len) {
					len = Genode::min(len, sizeof(ifr.ifr_name) - 1);
					Genode::memcpy(ifr.ifr_name, ptr, len);
					ifr.ifr_name[len] = 0;
				});
				Genode::log("using tap device \"", Genode::Cstring(ifr.ifr_name), "\"");
			} catch (...) {
				/* use tap0 if no config has been provided */
				Genode::copy_cstring(ifr.ifr_name, "tap0", sizeof(ifr.ifr_name));
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

		static Net::Mac_address
		_init_mac_address(Genode::Xml_node const &config)
		{
			try {
				return
					config.sub_node("nic").
						attribute_value("mac", default_mac_address());

			} catch (...) {

				return default_mac_address();
			}
		}

		void _handle_rx()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				size_t const max_pkt_size {
					Nic::Packet_allocator::DEFAULT_PACKET_SIZE };

				_drv_rx_handle_pkt(
					max_pkt_size,
					[&] (void   *conn_tx_pkt_base,
					     size_t &adjusted_conn_tx_pkt_size)
				{
					long int const read_result {
						read(_tap_fd, conn_tx_pkt_base, max_pkt_size) };

					if (read_result <= 0) {

						_rx_thread.blockade.wakeup();
						return Write_result::WRITE_FAILED;
					}
					adjusted_conn_tx_pkt_size = read_result;
					progress = true;
					return Write_result::WRITE_SUCCEEDED;
				});
			}
		}


		/************************
		 ** Uplink_client_base **
		 ************************/

		Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) override
		{
			int ret;

			/* non-blocking-write packet to TAP */
			do {
				ret = write(_tap_fd, conn_rx_pkt_base, conn_rx_pkt_size);
				/* drop packet if write would block */
				if (ret < 0 && errno == EAGAIN)
					continue;

				if (ret < 0) Genode::error("write: errno=", errno);
			} while (ret < 0);

			return Transmit_result::ACCEPTED;
		}

	public:

		Uplink_client(Genode::Env            &env,
		              Genode::Allocator      &alloc,
		              Genode::Xml_node const &config)
		:
			Uplink_client_base {
				env, alloc, _init_mac_address(config) },

			_tap_fd { _init_tap_fd(config) }
		{
			_drv_handle_link_state(true);
			_rx_thread.start();
		}
};


struct Server::Main
{
	Env                            &_env;
	Heap                            _heap       { _env.ram(), _env.rm() };
	Genode::Attached_rom_dataspace  _config_rom { _env, "config" };

	Main(Env &env) : _env(env)
	{
		Nic_driver_mode const mode {
			read_nic_driver_mode(_config_rom.xml()) };

		switch (mode) {
		case Nic_driver_mode::NIC_SERVER:
			{
				Nic::Root<Linux_session_component> &nic_root {
					*new (_heap) Nic::Root<Linux_session_component>(_env, _heap) };

				_env.parent().announce(_env.ep().manage(nic_root));
				break;
			}
		case Nic_driver_mode::UPLINK_CLIENT:

			new (_heap) Uplink_client(_env, _heap, _config_rom.xml());
			break;
		}
	}
};

void Component::construct(Genode::Env &env) { static Server::Main main(env); }
