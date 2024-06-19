/*
 * \brief  Routing rule for direct traffic between two interfaces
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DIRECT_RULE_H_
#define _DIRECT_RULE_H_

/* local includes */
#include <ipv4_address_prefix.h>
#include <list.h>

/* Genode includes */
#include <util/list.h>
#include <util/xml_node.h>

namespace Genode { class Xml_node; }

namespace Net {

	class                     Direct_rule_base;
	template <typename> class Direct_rule;
	template <typename> class Direct_rule_list;
}


class Net::Direct_rule_base
{
	protected:

		Ipv4_address_prefix const _dst;

	public:

		Direct_rule_base(Ipv4_address_prefix const dst) : _dst(dst) { }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const { Genode::print(output, "dst ", _dst); }


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address_prefix const &dst() const { return _dst; }
};


template <typename T>
struct Net::Direct_rule : Direct_rule_base,
                          Direct_rule_list<T>::Element
{
	Direct_rule(Ipv4_address_prefix const &dst) : Direct_rule_base(dst) { }
};


template <typename T>
struct Net::Direct_rule_list : List<T>
{
	using Base = List<T>;

	void
	find_longest_prefix_match(Ipv4_address const &ip,
	                          auto         const &handle_match,
	                          auto         const &handle_no_match) const
	{
		/*
		 * Simply handling the first match is sufficient as the list is
		 * sorted by the prefix size in descending order.
		 */
		for (T const *rule_ptr = Base::first();
		     rule_ptr != nullptr;
		     rule_ptr = rule_ptr->next()) {

			if (rule_ptr->dst().prefix_matches(ip)) {

				handle_match(*rule_ptr);
				return;
			}
		}
		handle_no_match();
	}

	void insert(T &rule)
	{
		/*
		 * Ensure that the list stays sorted by the prefix size in descending
		 * order.
		 */
		T *behind = nullptr;
		for (T *curr = Base::first(); curr; curr = curr->next()) {
			if (rule.dst().prefix >= curr->dst().prefix) {
				break; }

			behind = curr;
		}
		Base::insert(&rule, behind);
	}
};

#endif /* _RULE_H_ */
