/*
 * \brief  Interface back-end using Uplink sessions provided by the NIC router
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_SESSION_ROOT_H_
#define _UPLINK_SESSION_ROOT_H_

/* Genode includes */
#include <base/heap.h>
#include <root/component.h>
#include <nic/packet_allocator.h>
#include <uplink_session/rpc_object.h>

/* local includes */
#include <mac_allocator.h>
#include <interface.h>
#include <reference.h>
#include <report.h>
#include <session_env.h>
#include <communication_buffer.h>

namespace Net {

	class Uplink_session_component_base;
	class Uplink_session_component;
	class Uplink_session_root;
}


class Net::Uplink_session_component_base
{
	protected:

		Genode::Session_env  &_session_env;
		Genode::Heap          _alloc;
		Nic::Packet_allocator _packet_alloc;
		Communication_buffer  _tx_buf;
		Communication_buffer  _rx_buf;

	public:

		Uplink_session_component_base(Genode::Session_env       &session_env,
		                              Genode::size_t      const  tx_buf_size,
		                              Genode::size_t      const  rx_buf_size);
};


class Net::Uplink_session_component : private Uplink_session_component_base,
                                      public  ::Uplink::Session_rpc_object
{
	private:

		class Interface_policy : public Net::Interface_policy
		{
			private:

				Genode::Session_label    const  _label;
				Const_reference<Configuration>  _config;
				Genode::Session_env      const &_session_env;

			public:

				Interface_policy(Genode::Session_label const &label,
				                 Genode::Session_env   const &session_env,
				                 Configuration         const &config);


				/***************************
				 ** Net::Interface_policy **
				 ***************************/

				Domain_name determine_domain_name() const override;
				void handle_config(Configuration const &config) override { _config = config; }
				Genode::Session_label const &label() const override { return _label; }
				void report(Genode::Xml_generator &xml) const override { _session_env.report(xml); };
				void interface_unready() override { }
				void interface_ready() override { }
				bool interface_link_state() const override { return true; }
		};

		Interface_policy                       _interface_policy;
		Interface                              _interface;
		Genode::Ram_dataspace_capability const _ram_ds;

	public:

		Uplink_session_component(Genode::Session_env                    &session_env,
		                         Genode::size_t                   const  tx_buf_size,
		                         Genode::size_t                   const  rx_buf_size,
		                         Cached_timer                           &timer,
		                         Mac_address                      const  mac,
		                         Genode::Session_label            const &label,
		                         Interface_list                         &interfaces,
		                         Configuration                          &config,
		                         Genode::Ram_dataspace_capability const  ram_ds);


		/***************
		 ** Accessors **
		 ***************/

		Interface_policy           const &interface_policy() const { return _interface_policy; }
		Genode::Ram_dataspace_capability  ram_ds()           const { return _ram_ds; };
		Genode::Session_env        const &session_env()      const { return _session_env; };
};


class Net::Uplink_session_root
:
	public Genode::Root_component<Uplink_session_component>
{
	private:

		enum { MAC_ALLOC_BASE = 0x02 };

		Genode::Env              &_env;
		Cached_timer             &_timer;
		Reference<Configuration>  _config;
		Quota                    &_shared_quota;
		Interface_list           &_interfaces;

		void _invalid_downlink(char const *reason);


		/********************
		 ** Root_component **
		 ********************/

		Uplink_session_component *_create_session(char const *args) override;
		void _destroy_session(Uplink_session_component *session) override;

	public:

		Uplink_session_root(Genode::Env       &env,
		                    Cached_timer      &timer,
		                    Genode::Allocator &alloc,
		                    Configuration     &config,
		                    Quota             &shared_quota,
		                    Interface_list    &interfaces);

		void handle_config(Configuration &config) { _config = Reference<Configuration>(config); }
};

#endif /* _UPLINK_SESSION_ROOT_H_ */
