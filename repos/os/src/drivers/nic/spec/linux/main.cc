/*
 * \brief  NIC driver for Linux TUN/TAP device
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Martin Stein
 * \date   2011-08-08
 */

/*
 * Copyright (C) 2011-2021 Genode Labs GmbH
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
#include <os/reporter.h>

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>

/* Linux */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#pragma GCC diagnostic pop  /* restore -Wconversion warnings */


using namespace Genode;

using Tap_name    = String<IFNAMSIZ>;
using Mac_address = Net::Mac_address;


class Uplink_client : public Uplink_client_base
{
	private:

		struct Rx_signal_thread : Thread
		{
			int                       fd;
			Uplink_client &uplink;
			Blockade                  blockade { };

			Rx_signal_thread(Env &env, int fd, Uplink_client &uplink)
			: Thread(env, "rx_signal", 0x1000), fd(fd), uplink(uplink) { }

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
					uplink._rx_handler.local_submit();

					blockade.block();
				}
			}
		};

		int                           _tap_fd;
		Signal_handler<Uplink_client> _rx_handler { _env.ep(), *this, &Uplink_client::_handle_rx };
		Rx_signal_thread              _rx_thread  { _env, _tap_fd, *this };

		static int _init_tap_fd(Tap_name const &tap_name)
		{
			/* open TAP device */
			int ret;
			struct ifreq ifr;

			int fd = open("/dev/net/tun", O_RDWR);
			if (fd < 0) {
				error("could not open /dev/net/tun: no virtual network emulation");
				throw Exception();
			}

			/* set fd to non-blocking */
			if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
				error("could not set /dev/net/tun to non-blocking");
				throw Exception();
			}

			::memset(&ifr, 0, sizeof(ifr));
			ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

			copy_cstring(ifr.ifr_name, tap_name.string(), sizeof(ifr.ifr_name));
			log("using tap device \"", tap_name, "\"");

			ret = ioctl(fd, TUNSETIFF, (void *) &ifr);
			if (ret != 0) {
				error("could not configure /dev/net/tun: no virtual network emulation");
				close(fd);
				/* this error is fatal */
				throw Exception();
			}

			return fd;
		}

		void _handle_rx()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				size_t const max_pkt_size {
					Nic::Packet_allocator::OFFSET_PACKET_SIZE };

				_drv_rx_handle_pkt(
					max_pkt_size,
					[&] (void   *conn_tx_pkt_base,
					     size_t &adjusted_conn_tx_pkt_size)
				{
					ssize_t const read_result {
						::read(_tap_fd, conn_tx_pkt_base, max_pkt_size) };

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
			ssize_t ret;

			/* non-blocking-write packet to TAP */
			do {
				ret = ::write(_tap_fd, conn_rx_pkt_base, conn_rx_pkt_size);
				/* drop packet if write would block */
				if (ret < 0 && errno == EAGAIN)
					continue;

				if (ret < 0) error("write: errno=", errno);
			} while (ret < 0);

			return Transmit_result::ACCEPTED;
		}

	public:

		Uplink_client(Env               &env,
		              Allocator         &alloc,
		              Tap_name    const &tap_name,
		              Mac_address const &mac_address)
		:
			Uplink_client_base { env, alloc, mac_address },
			_tap_fd { _init_tap_fd(tap_name) }
		{
			_drv_handle_link_state(true);
			_rx_thread.start();
		}
};


struct Main
{
	Env                   &_env;
	Heap                   _heap       { _env.ram(), _env.rm() };
	Attached_rom_dataspace _config_rom { _env, "config" };

	Tap_name _tap_name {
		_config_rom.xml().attribute_value("tap", Tap_name("tap0")) };

	static Mac_address _default_mac_address()
	{
		Mac_address mac_addr { (uint8_t)0 };

		mac_addr.addr[0] = 0x02; /* unicast, locally managed MAC address */
		mac_addr.addr[5] = 0x01; /* arbitrary index */

		return mac_addr;
	}

	Mac_address _mac_address {
		_config_rom.xml().attribute_value("mac", _default_mac_address()) };

	Uplink_client _uplink { _env, _heap, _tap_name, _mac_address };

	Constructible<Reporter> _reporter { };

	Main(Env &env) : _env(env)
	{
		_config_rom.xml().with_optional_sub_node("report", [&] (Xml_node const &xml) {
			bool const report_mac_address =
				xml.attribute_value("mac_address", false);

			if (!report_mac_address)
				return;

			_reporter.construct(_env, "devices");
			_reporter->enabled(true);

			Reporter::Xml_generator report(*_reporter, [&] () {
				report.node("nic", [&] () {
					report.attribute("label", _tap_name);
					report.attribute("mac_address", String<32>(_mac_address));
				});
			});
		});
	}
};


void Component::construct(Env &env) { static Main main(env); }
