/*
 * \brief  Remember packets that wait for ARP replies at different interfaces
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ARP_WAITER_H_
#define _ARP_WAITER_H_

/* local includes */
#include <list.h>
#include <lazy_one_shot_timeout.h>
#include <assertion.h>

/* Genode includes */
#include <net/ipv4.h>
#include <util/list.h>
#include <nic_session/nic_session.h>

namespace Net {

	using Packet_descriptor = ::Nic::Packet_descriptor;
	class Interface;
	class Domain;
	class Configuration;
	class Arp_waiter;
	using Arp_waiter_list_element = Genode::List_element<Arp_waiter>;
	struct Packet_list_element;
	using Packet_list = Genode::List<Packet_list_element>;
	struct Arp_waiter_list;
}


struct Net::Packet_list_element : Genode::List<Packet_list_element>::Element
{
	Packet_descriptor const packet;

	Packet_list_element(Packet_descriptor const &packet) : packet(packet) { }
};


class Net::Arp_waiter
{
	private:

		Arp_waiter_list_element           _src_le;
		Interface                        &_src;
		Arp_waiter_list_element           _dst_le;
		Domain                           *_dst_ptr;
		Ipv4_address               const  _ip;
		Packet_list                       _packets { };
		Lazy_one_shot_timeout<Arp_waiter> _timeout;

		/*
		 * Noncopyable
		 */
		Arp_waiter(Arp_waiter const &);
		Arp_waiter &operator = (Arp_waiter const &);

		void _handle_timeout(Genode::Duration);

		void _dissolve();

	public:

		Arp_waiter(Interface                 &src,
		           Domain                    &dst,
		           Ipv4_address        const &ip,
		           Packet_list_element       &packet_le,
		           Genode::Microseconds       dissolve_timeout,
		           Cached_timer              &timer);

		~Arp_waiter();

		void handle_config(Domain &dst);

		void add_packet(Packet_list_element &le) { _packets.insert(&le); }

		void flush_packets(Genode::Deallocator &dealloc, auto const &fn)
		{
			while (Packet_list_element *le = _packets.first()) {
				_packets.remove(le);
				fn(le->packet);
				destroy(dealloc, le);
			}
		}


		/*********
		 ** Log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Interface          &src() const { return _src; }
		Ipv4_address const &ip()  const { return _ip; }
		Domain             &dst()       { return *_dst_ptr; }
};


struct Net::Arp_waiter_list : List<Arp_waiter_list_element>
{
	void find_by_ip(Ipv4_address const &ip, auto const &found_fn, auto const &not_found_fn)
	{
		bool found = false;
		for_each([&] (Arp_waiter_list_element &elem) {
			if (found)
				return;

			Arp_waiter &waiter = *elem.object();
			if (ip != waiter.ip())
				return;

			found_fn(waiter);
			found = true;
		});
		if (!found)
			not_found_fn();
	}
};

#endif /* _ARP_WAITER_H_ */
