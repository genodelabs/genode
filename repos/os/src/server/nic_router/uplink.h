/*
 * \brief  Uplink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_H_
#define _UPLINK_H_

/* Genode includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

/* local includes */
#include <avl_string_tree.h>
#include <interface.h>
#include <ipv4_address_prefix.h>

namespace Net {

	using Domain_name = Genode::String<160>;
	class Uplink_base;
	class Uplink;
	class Uplink_tree;
	class Uplink_interface_base;
	class Uplink_interface;
}


class Net::Uplink_tree
:
	public Avl_string_tree<Uplink, Genode::Session_label>
{ };


class Net::Uplink_base
{
	private:

		Genode::Session_label const _label;
		Domain_name           const _domain;

	public:

		Uplink_base(Genode::Xml_node const &node);

		virtual ~Uplink_base() { }


		/**************
		 ** Acessors **
		 **************/

		Genode::Session_label const &label()  const { return _label; }
		Domain_name           const &domain() const { return _domain; }
};


struct Net::Uplink : public  Uplink_base,
                     private Genode::Avl_string_base
{
	friend class Avl_string_tree<Uplink, Genode::Session_label>;
	friend class Genode::List<Uplink>;

	private:

		Genode::Allocator         &_alloc;
		Configuration       const &_config;
		Pointer<Uplink_interface>  _interface { };

		void _invalid(char const *reason) const;

	public:

		struct Invalid : Genode::Exception { };

		Uplink(Genode::Xml_node const &node,
		       Genode::Allocator      &alloc,
		       Uplink_tree            &old_uplinks,
		       Genode::Env            &env,
		       Timer::Connection      &timer,
		       Interface_list         &interfaces,
		       Configuration          &config);

		~Uplink();


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;
};


class Net::Uplink_interface_base : public Interface_policy
{
	private:

		Const_reference<Domain_name>  _domain_name;
		Genode::Session_label  const &_label;


		/***************************
		 ** Net::Interface_policy **
		 ***************************/

		Domain_name determine_domain_name() const override { return _domain_name(); };
		void handle_config(Configuration const &) override { }
		Genode::Session_label const &label() const override { return _label; }

	public:

		Uplink_interface_base(Domain_name           const &domain_name,
		                      Genode::Session_label const &label);

		virtual ~Uplink_interface_base() { }


		/***************
		 ** Accessors **
		 ***************/

		void domain_name(Domain_name const &v) { _domain_name = v; }
};


class Net::Uplink_interface : public Uplink_interface_base,
                              public Nic::Packet_allocator,
                              public Nic::Connection
{
	private:

		enum {
			PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE = Nic::Session::QUEUE_SIZE * PKT_SIZE,
		};

		bool                                     _link_state { false };
		Genode::Signal_handler<Uplink_interface> _link_state_handler;
		Net::Interface                           _interface;

		Ipv4_address_prefix _read_interface();

		void _handle_link_state();

	public:

		Uplink_interface(Genode::Env                 &env,
		                 Timer::Connection           &timer,
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

#endif /* _UPLINK_H_ */
