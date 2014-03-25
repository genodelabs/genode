/*
 * \brief  NIC driver for Linux TUN/TAP device
 * \author Stefan Kalkowski
 * \author Christian Helmuth
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
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <nic/component.h>

/* Linux */
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include <os/config.h>
#include <nic/xml_node.h>


class Linux_driver : public Nic::Driver
{
	private:

		struct Rx_thread : Genode::Thread<0x2000>
		{
			int          fd;
			Nic::Driver &driver;

			Rx_thread(int fd, Nic::Driver &driver)
			: Genode::Thread<0x2000>("rx"), fd(fd), driver(driver) { }

			void entry()
			{
				while (true) {
					/* wait for packet arrival on fd */
					int    ret;
					fd_set rfds;

					FD_ZERO(&rfds);
					FD_SET(fd, &rfds);
					do { ret = select(fd + 1, &rfds, 0, 0, 0); } while (ret < 0);

					/* inform driver about incoming packet */
					driver.handle_irq(fd);
				}
			}
		};

		Nic::Mac_address      _mac_addr;
		Nic::Rx_buffer_alloc &_alloc;

		char      _packet_buffer[1514];  /* maximum ethernet packet length */
		int       _tap_fd;
		Rx_thread _rx_thread;

		int _setup_tap_fd()
		{
			/* open TAP device */
			int ret;
			struct ifreq ifr;

			int fd = open("/dev/net/tun", O_RDWR);
			if (fd < 0) {
				PERR("could not open /dev/net/tun: no virtual network emulation");
				/* this error is fatal */
				throw Genode::Exception();
			}

			Genode::memset(&ifr, 0, sizeof(ifr));
			ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

			/* get tap device from config */
			try {
				Genode::Xml_node nic_node = Genode::config()->xml_node().sub_node("nic");
				nic_node.attribute("tap").value(ifr.ifr_name, sizeof(ifr.ifr_name));
				PINF("Using tap device \"%s\"", ifr.ifr_name);
			} catch (...) {
				/* use tap0 if no config has been provided */
				Genode::strncpy(ifr.ifr_name, "tap0", sizeof(ifr.ifr_name));
				PINF("No config provided, using tap0");
			}

			ret = ioctl(fd, TUNSETIFF, (void *) &ifr);
			if (ret != 0) {
				PERR("could not configure /dev/net/tun: no virtual network emulation");
				close(fd);
				/* this error is fatal */
				throw Genode::Exception();
			}

			return fd;
		}

	public:

		Linux_driver(Nic::Rx_buffer_alloc &alloc)
		: _alloc(alloc), _tap_fd(_setup_tap_fd()), _rx_thread(_tap_fd, *this)
		{
			/* try using configured MAC address */
			try {
				Genode::Xml_node nic_config = Genode::config()->xml_node().sub_node("nic");
				nic_config.attribute("mac").value(&_mac_addr);
				PINF("Using configured MAC address \"%02x:%02x:%02x:%02x:%02x:%02x\"",
						_mac_addr.addr[0],
						_mac_addr.addr[1],
						_mac_addr.addr[2],
						_mac_addr.addr[3],
						_mac_addr.addr[4],
						_mac_addr.addr[5]	);
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


		/***************************
		 ** Nic::Driver interface **
		 ***************************/

		Nic::Mac_address mac_address() { return _mac_addr; }

		void tx(char const *packet, Genode::size_t size)
		{
			int ret;

			/* blocking-write packet to TAP */
			do { ret = write(_tap_fd, packet, size); } while (ret < 0);
		}


		/******************************
		 ** Irq_activation interface **
		 ******************************/

		void handle_irq(int)
		{
			int ret;

			/* blocking read incoming packet */
			do {
				ret = read(_tap_fd, _packet_buffer, sizeof(_packet_buffer));
			} while (ret < 0);

			void *buffer = _alloc.alloc(ret);
			Genode::memcpy(buffer, _packet_buffer, ret);
			_alloc.submit();
		}
};


/*
 * Manually initialize the 'lx_environ' pointer, needed because 'nic_drv' is not
 * using the normal Genode startup code.
 */
extern char **environ;
char **lx_environ = environ;


int main(int, char **)
{
	using namespace Genode;

	printf("--- Linux/tap NIC driver started ---\n");

	/**
	 * Factory used by 'Nic::Root' at session creation/destruction time
	 */
	struct Linux_driver_factory : Nic::Driver_factory
	{
		Nic::Driver *create(Nic::Rx_buffer_alloc &alloc)
		{
			return new (env()->heap()) Linux_driver(alloc);
		}

		void destroy(Nic::Driver *driver)
		{
			Genode::destroy(env()->heap(), static_cast<Linux_driver *>(driver));
		}
	} driver_factory;

	enum { STACK_SIZE = 2*4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "nic_ep");

	static Nic::Root nic_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&nic_root));

	sleep_forever();
	return 0;
}

