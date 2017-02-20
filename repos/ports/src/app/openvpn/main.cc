/*
 * \brief  TUN/TAP to Nic_session interface
 * \author Josef Soentgen
 * \date   2014-06-05
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/heap.h>
#include <os/static_root.h>
#include <nic/component.h>
#include <root/component.h>
#include <libc/component.h>

/* libc includes */
#include <unistd.h>

/* local includes */
#include "tuntap.h"


/* external symbols provided by Genode's startup code */
extern char **genode_argv;
extern int    genode_argc;


/*********************************
 ** OpenVPN main thread wrapper **
 *********************************/

extern "C" int openvpn_main(int, char*[]);


class Openvpn_thread : public Genode::Thread
{
	private:

		int _argc;
		char **_argv;
		int _exitcode;

	public:

		Openvpn_thread(Genode::Env &env, int argc, char *argv[])
		:
			Thread(env, "openvpn_main", 16UL * 1024 * sizeof (long)),
			_argc(argc), _argv(argv),
			_exitcode(-1)
		{ }

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

class Openvpn_component : public Tuntap_device,
                          public Nic::Session_component
{
	private:

		Nic::Mac_address _mac_addr;

		char const *_packet;

		enum { READ = 0, WRITE = 1 };

		int _pipefd[2];
		Genode::Semaphore _startup_lock;
		Genode::Semaphore _tx_lock;

	protected:

		bool _send()
		{
			using namespace Genode;

			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.size()) {
				Genode::warning("invalid tx packet");
				return true;
			}

			_packet = _tx.sink()->packet_content(packet);

			/* notify openvpn */
			::write(_pipefd[WRITE], "1", 1);

			/* block while openvpn handles the packet */
			_tx_lock.down();
			_tx.sink()->acknowledge_packet(packet);

			return true;
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			while (_send()) ;
		}

	public:

		Openvpn_component(Genode::size_t const tx_buf_size,
		                  Genode::size_t const rx_buf_size,
		                  Genode::Allocator   &rx_block_md_alloc,
		                  Genode::Env         &env)
		: Session_component(tx_buf_size, rx_buf_size, rx_block_md_alloc, env)
		{
			char buf[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
			_mac_addr = Nic::Mac_address((void*)buf);
			if (pipe(_pipefd)) {
				Genode::error("could not create pipe");
				throw Genode::Exception();
			}
		}

		/**************************************
		 ** Nic::Session_component interface **
		 **************************************/

		Nic::Mac_address mac_address() override { return _mac_addr; }

		bool link_state() override
		{
			/* XXX always return true for now */
			return true;
		}

		/***********************
		 ** TUN/TAP interface **
		 ***********************/

		int fd() { return _pipefd[READ]; }

		/* tx */
		int read(char *buf, Genode::size_t len)
		{
			Genode::memcpy(buf, _packet, len);
			_packet = 0;

			/* unblock nic client */
			_tx_lock.up();

			return len;
		}

		/* rx */
		int write(char const *buf, Genode::size_t len)
		{
			_handle_packet_stream();

			if (!_rx.source()->ready_to_submit())
				return 0;

			try {
				Genode::Packet_descriptor packet = _rx.source()->alloc_packet(len);
				Genode::memcpy(_rx.source()->packet_content(packet), buf, len);
				_rx.source()->submit_packet(packet);
			} catch (...) { return 0; }

			return len;
		}

		void up() { _startup_lock.up(); }

		void down() { _startup_lock.down(); }
};


class Root : public Genode::Root_component<Openvpn_component, Genode::Single_client>
{
	private:

		Libc::Env          &_env;
		Genode::Heap        _heap { _env.ram(), _env.rm() };
		Openvpn_thread     *_thread = nullptr;

	protected:

		Openvpn_component *_create_session(const char *args)
		{
			using namespace Genode;
			using Genode::size_t;

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (unsigned long)sizeof(Openvpn_component));
			if (ram_quota < session_size)
				throw Genode::Root::Quota_exceeded();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota - session_size) {
				Genode::error("insufficient 'ram_quota', got %ld, need %ld",
				              ram_quota, tx_buf_size + rx_buf_size + session_size);
				throw Genode::Root::Quota_exceeded();
			}

			Openvpn_component *component = new (Root::md_alloc())
			                               Openvpn_component(tx_buf_size,
			                                                 rx_buf_size,
			                                                 _heap, _env);
			/**
			 * Setting the pointer in this manner is quite hackish but it has
			 * to be valid before OpenVPN calls open_tun(), which unfortunatly
			 * is early.
			 */
			_tuntap_dev = component;

			_thread = new (_heap) Openvpn_thread(_env, genode_argc, genode_argv);
			_thread->start();

			/* wait until OpenVPN configured the TUN/TAP device for the first time */
			_tuntap_dev->down();

			return component;
		}

		void _destroy_session(Openvpn_component *session)
		{
			Genode::destroy(Root::md_alloc(), session);
			Genode::destroy(Root::md_alloc(), _thread);
			_thread = nullptr;
		}

	public:

		Root(Libc::Env &env)
		: Genode::Root_component<Openvpn_component, Genode::Single_client>(env.ep(), _heap),
			_env(env)
		{ }
};


/***************
 ** Component **
 ***************/

void Libc::Component::construct(Libc::Env &env)
{
	static ::Root nic_root(env);
	env.parent().announce(env.ep().manage(nic_root));
}
