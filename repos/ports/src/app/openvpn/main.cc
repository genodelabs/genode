/**
 * \brief  TUN/TAP to Nic_session interface
 * \author Josef Soentgen
 * \date   2014-06-05
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <os/config.h>
#include <os/static_root.h>
#include <cap_session/connection.h>
#include <nic/component.h>
#include <root/component.h>

/* libc includes */
#include <unistd.h>

/* local includes */
#include "tuntap.h"


static int const verbose = false;
#define PDBGV(...) if (verbose) PDBG(__VA_ARGS__)


/* external symbols provided by Genode's startup code */
extern char **genode_argv;
extern int    genode_argc;


/*********************************
 ** OpenVPN main thread wrapper **
 *********************************/

extern "C" int openvpn_main(int, char*[]);


class Openvpn_thread : public Genode::Thread<16UL * 1024 * sizeof (long)>
{
	private:

		int _argc;
		char **_argv;
		int _exitcode;

	public:

		Openvpn_thread(int argc, char *argv[])
		:
			Thread("openvpn_main"),
			_argc(argc), _argv(argv),
			_exitcode(-1)
		{
			//for (int i = 0; i < _argc; i++)
			//	PINF("_argv[%i]: '%s'", i, _argv[i]);
		}

		void entry()
		{
			_exitcode = ::openvpn_main(_argc, _argv);
		};
};


static Tuntap_device* _tuntap_dev;


Tuntap_device *tuntap_dev()
{
	return _tuntap_dev;
}


/***************************************
 ** Implementation of the Nic service **
 ***************************************/

class Nic_driver : public Tuntap_device,
                   public Nic::Driver
{
	private:

		Nic::Mac_address      _mac_addr {{ 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 }};
		Nic::Rx_buffer_alloc &_alloc;

		char const *_packet;

		enum { READ = 0, WRITE = 1 };

		int _pipefd[2];
		Genode::Semaphore _startup_lock;
		Genode::Semaphore _tx_lock;


	public:

		Nic_driver(Nic::Rx_buffer_alloc &alloc)
		:
			_alloc(alloc),
			_packet(0)
		{
			if (pipe(_pipefd)) {
				PERR("could not create pipe");
				throw Genode::Exception();
			}
		}

		~Nic_driver() { PDBG("should probably be implemented"); }


		/***************************
		 ** Nic::Driver interface **
		 ***************************/

		Nic::Mac_address mac_address() { return _mac_addr; }

		void tx(char const *packet, Genode::size_t size)
		{
			PDBGV("packet:0x%p size:%zu", packet, size);

			_packet = packet;

			/* notify openvpn */
			::write(_pipefd[WRITE], "1", 1);

			/* block while openvpn handles the packet */
			_tx_lock.down();
		}

		/******************************
		 ** Irq_activation interface **
		 ******************************/

		void handle_irq(int) { }

		/***********************
		 ** TUN/TAP interface **
		 ***********************/

		int fd() { return _pipefd[READ]; }

		/* tx */
		int read(char *buf, Genode::size_t len)
		{
			PDBGV("buf:0x%p len:%zu", len);

			Genode::memcpy(buf, _packet, len);
			_packet = 0;

			/* unblock nic client */
			_tx_lock.up();

			return len;
		}

		/* rx */
		int write(char const *buf, Genode::size_t len)
		{
			PDBGV("buf:0x%p len:%zu", len);

			void *buffer = _alloc.alloc(len);
			Genode::memcpy(buffer, buf, len);
			_alloc.submit();

			return len;
		}

		void up() { _startup_lock.up(); }

		void down() { _startup_lock.down(); }
};


struct Main
{
	struct Nic_driver_factory : Nic::Driver_factory
	{
		Nic_driver     *drv { 0 };
		Openvpn_thread *openvpn { 0 };

		Nic::Driver *create(Nic::Rx_buffer_alloc &alloc)
		{
			/* there can be only one */
			if (!drv) {
				drv = new (Genode::env()->heap()) Nic_driver(alloc);

				/**
				 * Setting the pointer in this manner is quite hackish but it has
				 * to be valid before OpenVPN calls open_tun(), which unfortunatly
				 * is early.
				 */
				_tuntap_dev = drv;

				PDBGV("start OpenVPN main thread");
				Openvpn_thread *openvpn = new (Genode::env()->heap()) Openvpn_thread(genode_argc,
				                                                                     genode_argv);

				openvpn->start();

				/* wait until OpenVPN configured the TUN/TAP device for the first time */
				_tuntap_dev->down();

				return drv;
			}

			return 0;
		}

		void destroy(Nic::Driver *driver)
		{
			Genode::destroy(Genode::env()->heap(), static_cast<Nic_driver *>(driver));
			drv = 0;
			Genode::destroy(Genode::env()->heap(), openvpn);
			openvpn = 0;
		}
	} driver_factory;

	Server::Entrypoint &ep;

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		static Nic::Root nic_root(&ep.rpc_ep(), Genode::env()->heap(), driver_factory);

		Genode::env()->parent()->announce(ep.manage(nic_root));
	}
};


/**********************
 ** Server framework **
 **********************/

namespace Server {
	char const *name()             { return "openvpn_ep"; }
	size_t stack_size()            { return 8 * 1024 * sizeof (addr_t); }
	void construct(Entrypoint &ep) { static Main server(ep); }
}
