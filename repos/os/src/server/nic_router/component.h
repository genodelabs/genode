/*
 * \brief  Downlink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode includes */
#include <base/allocator_guard.h>
#include <root/component.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>

/* local includes */
#include <mac_allocator.h>
#include <interface.h>
#include <reference.h>

namespace Net {

	class Communication_buffer;
	class Session_component_base;
	class Session_component;
	class Root;
}


class Net::Communication_buffer : public Genode::Ram_dataspace_capability
{
	private:

		Genode::Ram_session &_ram;

	public:

		Communication_buffer(Genode::Ram_session  &ram,
		                     Genode::size_t const  size);

		~Communication_buffer() { _ram.free(*this); }
};


class Net::Session_component_base
{
	protected:

		Genode::Allocator_guard _guarded_alloc;
		Nic::Packet_allocator   _range_alloc;
		Communication_buffer    _tx_buf;
		Communication_buffer    _rx_buf;

	public:

		Session_component_base(Genode::Allocator           &guarded_alloc_backing,
		                       Genode::size_t        const  guarded_alloc_amount,
		                       Genode::Ram_session         &buf_ram,
		                       Genode::size_t        const  tx_buf_size,
		                       Genode::size_t        const  rx_buf_size);
};


class Net::Session_component : private Session_component_base,
                               public  ::Nic::Session_rpc_object
{
	private:

		struct Interface_policy : Net::Interface_policy
		{
			private:

				Genode::Session_label    const _label;
				Const_reference<Configuration> _config;

			public:

				Interface_policy(Genode::Session_label const &label,
				                 Configuration         const &config);


				/***************************
				 ** Net::Interface_policy **
				 ***************************/

				Domain_name determine_domain_name() const override;
				void handle_config(Configuration const &config) override { _config = config; }
		};

		bool             _link_state { true };
		Interface_policy _interface_policy;
		Interface        _interface;

	public:

		Session_component(Genode::Allocator           &alloc,
		                  Timer::Connection           &timer,
		                  Genode::size_t        const  amount,
		                  Genode::Ram_session         &buf_ram,
		                  Genode::size_t        const  tx_buf_size,
		                  Genode::size_t        const  rx_buf_size,
		                  Genode::Region_map          &region_map,
		                  Mac_address           const  mac,
		                  Genode::Entrypoint          &ep,
		                  Mac_address           const &router_mac,
		                  Genode::Session_label const &label,
		                  Interface_list              &interfaces,
		                  Configuration               &config);


		/******************
		 ** Nic::Session **
		 ******************/

		Mac_address mac_address() override { return _interface.mac(); }
		bool link_state() override { return _interface.link_state(); }
		void link_state_sigh(Genode::Signal_context_capability sigh) override {
			_interface.session_link_state_sigh(sigh); }
};


class Net::Root : public Genode::Root_component<Session_component>
{
	private:

		enum { MAC_ALLOC_BASE = 0x02 };

		Timer::Connection        &_timer;
		Mac_allocator             _mac_alloc;
		Genode::Entrypoint       &_ep;
		Mac_address const         _router_mac;
		Reference<Configuration>  _config;
		Genode::Ram_session      &_buf_ram;
		Genode::Region_map       &_region_map;
		Interface_list           &_interfaces;


		/********************
		 ** Root_component **
		 ********************/

		Session_component *_create_session(char const *args) override;

	public:

		Root(Genode::Entrypoint  &ep,
		     Timer::Connection   &timer,
		     Genode::Allocator   &alloc,
		     Configuration       &config,
		     Genode::Ram_session &buf_ram,
		     Interface_list      &interfaces,
		     Genode::Region_map  &region_map);

		void handle_config(Configuration &config) { _config = Reference<Configuration>(config); }
};

#endif /* _COMPONENT_H_ */
