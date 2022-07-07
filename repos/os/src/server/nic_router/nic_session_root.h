/*
 * \brief  Interface back-end using NIC sessions provided by the NIC router
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NIC_SESSION_ROOT_H_
#define _NIC_SESSION_ROOT_H_

/* Genode includes */
#include <base/heap.h>
#include <root/component.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>

/* local includes */
#include <mac_allocator.h>
#include <interface.h>
#include <reference.h>
#include <report.h>
#include <session_env.h>
#include <communication_buffer.h>

namespace Net {

	class Nic_session_component_base;
	class Nic_session_component;
	class Nic_session_root;
}


class Net::Nic_session_component_base
{
	protected:

		Genode::Session_env  &_session_env;
		Genode::Heap          _alloc;
		Nic::Packet_allocator _packet_alloc;
		Communication_buffer  _tx_buf;
		Communication_buffer  _rx_buf;

	public:

		Nic_session_component_base(Genode::Session_env       &session_env,
		                           Genode::size_t      const  tx_buf_size,
		                           Genode::size_t      const  rx_buf_size);
};


class Net::Nic_session_component : private Nic_session_component_base,
                                   public  ::Nic::Session_rpc_object
{
	private:

		class Interface_policy : public Net::Interface_policy
		{
			private:

				using Signal_context_capability =
					Genode::Signal_context_capability;

				/*
				 * The transient link state is a combination of session and
				 * interface link state. The first word in the value name
				 * denotes the link state of the session. If the session
				 * link state has already been read by the client and
				 * can therefore be altered directly, it is marked as
				 * 'ACKNOWLEDGED'. Otherwise, the denoted session state
				 * has to stay fixed until the client has read it. In this
				 * case, the session link state in the value name may be
				 * followed by the pending link state edges. Consequently,
				 * the last 'UP' or 'DOWN' in each value name denotes the
				 * router-internal interface link-state.
				 */
				enum Transient_link_state
				{
					DOWN_ACKNOWLEDGED,
					DOWN,
					DOWN_UP,
					DOWN_UP_DOWN,
					UP_ACKNOWLEDGED,
					UP,
					UP_DOWN,
					UP_DOWN_UP
				};

				Genode::Session_label    const  _label;
				Const_reference<Configuration>  _config;
				Genode::Session_env      const &_session_env;
				Transient_link_state            _transient_link_state    { DOWN_ACKNOWLEDGED };
				Signal_context_capability       _session_link_state_sigh { };

				void _session_link_state_transition(Transient_link_state tls);

			public:

				Interface_policy(Genode::Session_label const &label,
				                 Genode::Session_env   const &session_env,
				                 Configuration         const &config);

				bool read_and_ack_session_link_state();

				void session_link_state_sigh(Genode::Signal_context_capability sigh);


				/***************************
				 ** Net::Interface_policy **
				 ***************************/

				Domain_name determine_domain_name() const override;
				void handle_config(Configuration const &config) override { _config = config; }
				Genode::Session_label const &label() const override { return _label; }
				void report(Genode::Xml_generator &xml) const override { _session_env.report(xml); };
				void interface_unready() override;
				void interface_ready() override;
				bool interface_link_state() const override;
		};

		Interface_policy                       _interface_policy;
		Interface                              _interface;
		Genode::Ram_dataspace_capability const _ram_ds;

	public:

		Nic_session_component(Genode::Session_env                    &session_env,
		                      Genode::size_t                   const  tx_buf_size,
		                      Genode::size_t                   const  rx_buf_size,
		                      Cached_timer                           &timer,
		                      Mac_address                      const  mac,
		                      Mac_address                      const &router_mac,
		                      Genode::Session_label            const &label,
		                      Interface_list                         &interfaces,
		                      Configuration                          &config,
		                      Genode::Ram_dataspace_capability const  ram_ds);


		/******************
		 ** Nic::Session **
		 ******************/

		Mac_address mac_address() override { return _interface.mac(); }
		bool link_state() override;
		void link_state_sigh(Genode::Signal_context_capability sigh) override;


		/***************
		 ** Accessors **
		 ***************/

		Interface_policy           const &interface_policy() const { return _interface_policy; }
		Genode::Ram_dataspace_capability  ram_ds()           const { return _ram_ds; };
		Genode::Session_env        const &session_env()      const { return _session_env; };
};


class Net::Nic_session_root
:
	public Genode::Root_component<Nic_session_component>
{
	private:

		enum { MAC_ALLOC_BASE = 0x02 };

		Genode::Env              &_env;
		Cached_timer             &_timer;
		Mac_allocator             _mac_alloc;
		Mac_address        const  _router_mac;
		Reference<Configuration>  _config;
		Quota                    &_shared_quota;
		Interface_list           &_interfaces;

		void _invalid_downlink(char const *reason);


		/********************
		 ** Root_component **
		 ********************/

		Nic_session_component *_create_session(char const *args) override;
		void _destroy_session(Nic_session_component *session) override;

	public:

		Nic_session_root(Genode::Env       &env,
		                 Cached_timer      &timer,
		                 Genode::Allocator &alloc,
		                 Configuration     &config,
		                 Quota             &shared_quota,
		                 Interface_list    &interfaces);

		void handle_config(Configuration &config) { _config = Reference<Configuration>(config); }
};

#endif /* _NIC_SESSION_ROOT_H_ */
