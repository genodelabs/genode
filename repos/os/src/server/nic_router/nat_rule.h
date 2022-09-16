/*
 * \brief  Rule for doing NAT from one given interface to another
 * \author Martin Stein
 * \date   2016-09-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NAT_RULE_H_
#define _NAT_RULE_H_

/* local includes */
#include <port_allocator.h>
#include <l3_protocol.h>
#include <avl_tree.h>

/* Genode includes */
#include <util/avl_string.h>

namespace Net {

	class Domain;
	class Domain_dict;

	class Port_allocator;
	class Nat_rule_base;
	class Nat_rule;
	class Nat_rule_tree;
}


class Net::Nat_rule : public Genode::Avl_node<Nat_rule>
{
	private:

		Domain               &_domain;
		Port_allocator_guard  _tcp_port_alloc;
		Port_allocator_guard  _udp_port_alloc;
		Port_allocator_guard  _icmp_port_alloc;

	public:

		struct Invalid : Genode::Exception { };

		Nat_rule(Domain_dict            &domains,
		         Port_allocator         &tcp_port_alloc,
		         Port_allocator         &udp_port_alloc,
		         Port_allocator         &icmp_port_alloc,
		         Genode::Xml_node const  node,
		         bool             const  verbose);

		Nat_rule &find_by_domain(Domain &domain);

		template <typename HANDLE_MATCH_FN,
		          typename HANDLE_NO_MATCH_FN>

		void find_by_domain(Domain                &domain,
		                    HANDLE_MATCH_FN    &&  handle_match,
		                    HANDLE_NO_MATCH_FN &&  handle_no_match)
		{
			if (&domain != &_domain) {

				Nat_rule *const rule_ptr {
					Avl_node<Nat_rule>::child(
						(Genode::addr_t)&domain > (Genode::addr_t)&_domain) };

				if (rule_ptr != nullptr) {

					rule_ptr->find_by_domain(
						domain, handle_match, handle_no_match);

				} else {

					handle_no_match();
				}

			} else {

				handle_match(*this);
			}
		}

		Port_allocator_guard &port_alloc(L3_protocol const prot);


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Nat_rule *rule);


		/***************
		 ** Accessors **
		 ***************/

		Domain               &domain()    const { return _domain; }
		Port_allocator_guard &tcp_port_alloc()  { return _tcp_port_alloc; }
		Port_allocator_guard &udp_port_alloc()  { return _udp_port_alloc; }
		Port_allocator_guard &icmp_port_alloc() { return _icmp_port_alloc; }
};


struct Net::Nat_rule_tree : Avl_tree<Nat_rule>
{
	template <typename HANDLE_MATCH_FN,
	          typename HANDLE_NO_MATCH_FN>

	void find_by_domain(Domain                &domain,
	                    HANDLE_MATCH_FN    &&  handle_match,
	                    HANDLE_NO_MATCH_FN &&  handle_no_match)
	{
		if (first() != nullptr) {

			first()->find_by_domain(domain, handle_match, handle_no_match);

		} else {

			handle_no_match();
		}
	}
};

#endif /* _NAT_RULE_H_ */
