/*
 * \brief  Utilities for handling DNS configurations
 * \author Martin Stein
 * \date   2020-11-17
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DNS_H_
#define _DNS_H_

/* local includes */
#include <list.h>
#include <pointer.h>

/* Genode includes */
#include <util/reconstructible.h>
#include <net/ipv4.h>
#include <net/dhcp.h>

namespace Genode {

	class Xml_attribute;
}

namespace Net {

	class Dns_server;
	class Dns_domain_name;

	using Dns_server_list = Net::List<Dns_server>;
}

class Net::Dns_server : private Genode::Noncopyable,
                        public  Net::List<Dns_server>::Element
{
	private:

		Net::Ipv4_address const _ip;

	public:

		struct Invalid : Genode::Exception { };

		Dns_server(Net::Ipv4_address const &ip);

		bool equal_to(Dns_server const &server) const;


		/***************
		 ** Accessors **
		 ***************/

		Net::Ipv4_address const &ip() const { return _ip; }
};

class Net::Dns_domain_name : private Genode::Noncopyable
{
	private:

		enum { STRING_CAPACITY = 160 };

	public:

		using String = Genode::String<STRING_CAPACITY>;

	private:

		Genode::Allocator &_alloc;
		Pointer<String>    _string { };

	public:

		Dns_domain_name(Genode::Allocator &alloc);

		~Dns_domain_name();

		void set_to(Dns_domain_name const &name);

		void set_to(Genode::Xml_attribute const &name_attr);

		void set_to(Dhcp_packet::Domain_name const &name_option);

		void set_invalid();

		bool valid() const { return _string.valid(); }

		template <typename FUNC>
		void with_string(FUNC && func) const
		{
			if (_string.valid()) {
				func(_string());
			}
		}

		bool equal_to(Dns_domain_name const &other) const;
};

#endif /* _DNS_H_ */
