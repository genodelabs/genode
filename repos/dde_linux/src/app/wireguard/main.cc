/*
 * \brief  Wireguard component
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <nic_connection.h>
#include <uplink_connection.h>

/* base includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/session_label.h>
#include <timer_session/connection.h>

/* lx-kit includes */
#include <lx_kit/env.h>

/* lx-emul includes */
#include <lx_emul/init.h>

/* lx-user includes */
#include <lx_user/io.h>

/* app/wireguard includes */
#include <genode_c_api/wireguard.h>
#include <base64.h>
#include <config_model.h>

using namespace Genode;

namespace Wireguard { class  Main; }


class Wireguard::Main : private Entrypoint::Io_progress_handler,
                        private Nic_connection_notifier
{
	private:

		Env                              &_env;
		Timer::Connection                 _timer                 { _env };
		Heap                              _heap                  { _env.ram(), _env.rm() };
		Attached_rom_dataspace            _config_rom            { _env, "config" };
		Signal_handler<Main>              _config_handler        { _env.ep(), *this, &Main::_handle_config };
		Signal_handler<Main>              _signal_handler        { _env.ep(), *this, &Main::_handle_signal };
		Config_model                      _config_model          { _heap };
		Signal_handler<Main>              _nic_ip_config_handler { _env.ep(), *this, &Main::_handle_nic_ip_config };
		Nic_connection                    _nic_connection        { _env, _heap, _signal_handler, _config_rom.xml(), _timer, *this };
		Constructible<Uplink_connection>  _uplink_connection     { };

		void _handle_signal()
		{
			lx_user_handle_io();
			Lx_kit::env().scheduler.execute();
		}

		void _handle_config() { _config_rom.update(); }

		void _handle_nic_ip_config();


		/*****************************
		 ** Nic_connection_notifier **
		 *****************************/

		void notify_about_ip_config_update() override
		{
			_nic_ip_config_handler.local_submit();
		}

	public:

		Main(Env &env)
		:
			_env(env)
		{
			Lx_kit::initialize(_env, _signal_handler);

			/*
			 * We have to call the static constructors because otherwise the
			 * initcall list of the LX kit won't get populated.
			 */
			_env.exec_static_constructors();

			_config_rom.sigh(_config_handler);
			_handle_config();

			env.ep().register_io_progress_handler(*this);

			/* trigger signal handling once after construction */
			Signal_transmitter(_signal_handler).submit();
		}

		/**
		 * Entrypoint::Io_progress_handler
		 */
		void handle_io_progress() override
		{
			if (_uplink_connection.constructed()) {
				_uplink_connection->notify_peer();
			}
			_nic_connection.notify_peer();
		}

		void update(genode_wg_config_callbacks & callbacks)
		{
			_config_model.update(callbacks, _config_rom.xml());
		}

		void net_receive(genode_wg_uplink_connection_receive_t uplink_rcv_callback,
		                 genode_wg_nic_connection_receive_t    nic_rcv_callback)
		{
			if (_uplink_connection.constructed()) {
				_uplink_connection->for_each_rx_packet(uplink_rcv_callback);
			}
			_nic_connection.for_each_rx_packet(nic_rcv_callback);
		}

		void send_wg_prot_at_nic_connection(
			genode_wg_u8_t const *wg_prot_base,
			genode_wg_size_t      wg_prot_size,
			genode_wg_u16_t       udp_src_port_big_endian,
			genode_wg_u16_t       udp_dst_port_big_endian,
			genode_wg_u32_t       ipv4_src_addr_big_endian,
			genode_wg_u32_t       ipv4_dst_addr_big_endian,
			genode_wg_u8_t        ipv4_dscp_ecn,
			genode_wg_u8_t        ipv4_ttl);

		void send_ip_at_uplink_connection(
			genode_wg_u8_t const *ip_base,
			genode_wg_size_t      ip_size);
};


void Wireguard::Main::_handle_nic_ip_config()
{
	if (_nic_connection.ip_config().valid()) {
		if (!_uplink_connection.constructed()) {
			_uplink_connection.construct(_env, _heap, _signal_handler);
		}
	} else {
		if (_uplink_connection.constructed()) {
			_uplink_connection.destruct();
		}
	}
}


void Wireguard::Main::send_wg_prot_at_nic_connection(
	genode_wg_u8_t const *wg_prot_base,
	genode_wg_size_t      wg_prot_size,
	genode_wg_u16_t       udp_src_port_big_endian,
	genode_wg_u16_t       udp_dst_port_big_endian,
	genode_wg_u32_t       ipv4_src_addr_big_endian,
	genode_wg_u32_t       ipv4_dst_addr_big_endian,
	genode_wg_u8_t        ipv4_dscp_ecn,
	genode_wg_u8_t        ipv4_ttl)
{
	_nic_connection.send_wg_prot(
		wg_prot_base,
		wg_prot_size,
		udp_src_port_big_endian,
		udp_dst_port_big_endian,
		ipv4_src_addr_big_endian,
		ipv4_dst_addr_big_endian,
		ipv4_dscp_ecn,
		ipv4_ttl);
}


void Wireguard::Main::send_ip_at_uplink_connection(
	genode_wg_u8_t const *ip_base,
	genode_wg_size_t      ip_size)
{
	if (_uplink_connection.constructed()) {
		_uplink_connection->send_ip(ip_base, ip_size);
	} else {
		log("Main: drop packet - uplink connection down");
	}
}


static Wireguard::Main & main_object(Genode::Env & env)
{
	static Wireguard::Main main { env };
	return main;
}


extern "C" void
genode_wg_update_config(struct genode_wg_config_callbacks * callbacks)
{
	main_object(Lx_kit::env().env).update(*callbacks);
};


extern "C" void
genode_wg_net_receive(genode_wg_uplink_connection_receive_t uplink_rcv_callback,
                      genode_wg_nic_connection_receive_t    nic_rcv_callback)
{
	main_object(Lx_kit::env().env).net_receive(uplink_rcv_callback,
	                                           nic_rcv_callback);
}


void genode_wg_send_wg_prot_at_nic_connection(
	genode_wg_u8_t const *wg_prot_base,
	genode_wg_size_t      wg_prot_size,
	genode_wg_u16_t       udp_src_port_big_endian,
	genode_wg_u16_t       udp_dst_port_big_endian,
	genode_wg_u32_t       ipv4_src_addr_big_endian,
	genode_wg_u32_t       ipv4_dst_addr_big_endian,
	genode_wg_u8_t        ipv4_dscp_ecn,
	genode_wg_u8_t        ipv4_ttl)
{
	main_object(Lx_kit::env().env).send_wg_prot_at_nic_connection(
		wg_prot_base,
		wg_prot_size,
		udp_src_port_big_endian,
		udp_dst_port_big_endian,
		ipv4_src_addr_big_endian,
		ipv4_dst_addr_big_endian,
		ipv4_dscp_ecn,
		ipv4_ttl);
}


void genode_wg_send_ip_at_uplink_connection(
	genode_wg_u8_t const *ip_base,
	genode_wg_size_t      ip_size)
{
	main_object(Lx_kit::env().env).send_ip_at_uplink_connection(
		ip_base,
		ip_size);
}


void Component::construct(Env &env)
{
	main_object(env);

	/*
	 * Main needs to be constructed before startin Linux code,
	 * because of genode_wg_* calls
	 */
	lx_emul_start_kernel(nullptr);
}
