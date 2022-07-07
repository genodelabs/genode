/*
 * \brief  Interface back-end using a NIC session requested by the NIC router
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NIC_CLIENT_H_
#define _NIC_CLIENT_H_

/* Genode includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

/* local includes */
#include <avl_string_tree.h>
#include <interface.h>
#include <ipv4_address_prefix.h>

namespace Net {

	using Domain_name = Genode::String<160>;
	class Nic_client_base;
	class Nic_client;
	class Nic_client_tree;
	class Nic_client_interface_base;
	class Nic_client_interface;
}


class Net::Nic_client_tree
:
	public Avl_string_tree<Nic_client, Genode::Session_label>
{ };


class Net::Nic_client_base
{
	private:

		Genode::Session_label const _label;
		Domain_name           const _domain;

	public:

		Nic_client_base(Genode::Xml_node const &node);

		virtual ~Nic_client_base() { }


		/**************
		 ** Acessors **
		 **************/

		Genode::Session_label const &label()  const { return _label; }
		Domain_name           const &domain() const { return _domain; }
};


class Net::Nic_client : public  Nic_client_base,
                        private Genode::Avl_string_base
{
	friend class Avl_string_tree<Nic_client, Genode::Session_label>;
	friend class Genode::List<Nic_client>;

	private:

		Genode::Allocator             &_alloc;
		Configuration           const &_config;
		Pointer<Nic_client_interface>  _interface { };

		void _invalid(char const *reason) const;

	public:

		struct Invalid : Genode::Exception { };

		Nic_client(Genode::Xml_node const &node,
		           Genode::Allocator      &alloc,
		           Nic_client_tree        &old_nic_clients,
		           Genode::Env            &env,
		           Cached_timer           &timer,
		           Interface_list         &interfaces,
		           Configuration          &config);

		~Nic_client();


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;
};


class Net::Nic_client_interface_base : public Interface_policy
{
	private:

		Const_reference<Domain_name>  _domain_name;
		Genode::Session_label  const  _label;
		bool                   const &_session_link_state;
		bool                          _interface_ready { false };


		/***************************
		 ** Net::Interface_policy **
		 ***************************/

		Domain_name determine_domain_name() const override { return _domain_name(); };
		void handle_config(Configuration const &) override { }
		Genode::Session_label const &label() const override { return _label; }
		void interface_unready() override;
		void interface_ready() override;
		bool interface_link_state() const override;

	public:

		Nic_client_interface_base(Domain_name           const &domain_name,
		                          Genode::Session_label const &label,
		                          bool                  const &session_link_state);

		virtual ~Nic_client_interface_base() { }


		/***************
		 ** Accessors **
		 ***************/

		void domain_name(Domain_name const &v) { _domain_name = v; }
};


class Net::Nic_client_interface : public Nic_client_interface_base,
                                  public Nic::Packet_allocator,
                                  public Nic::Connection
{
	private:

		enum {
			PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE = Nic::Session::QUEUE_SIZE * PKT_SIZE,
		};

		bool                                         _session_link_state { false };
		Genode::Signal_handler<Nic_client_interface> _session_link_state_handler;
		Net::Interface                               _interface;

		Ipv4_address_prefix _read_interface();

		void _handle_session_link_state();

	public:

		Nic_client_interface(Genode::Env                 &env,
		                     Cached_timer                &timer,
		                     Genode::Allocator           &alloc,
		                     Interface_list              &interfaces,
		                     Configuration               &config,
		                     Domain_name           const &domain_name,
		                     Genode::Session_label const &label);


		/***************
		 ** Accessors **
		 ***************/

		Mac_address const &router_mac() const { return _interface.router_mac(); }
};

#endif /* _NIC_CLIENT_H_ */
