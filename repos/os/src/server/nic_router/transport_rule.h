/*
 * \brief  Rule for allowing direct TCP/UDP traffic between two interfaces
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRANSPORT_RULE_H_
#define _TRANSPORT_RULE_H_

/* local includes */
#include <direct_rule.h>
#include <permit_rule.h>

namespace Genode { class Allocator; }

namespace Net {

	class Configuration;
	class Transport_rule;
	class Transport_rule_list;
}


class Net::Transport_rule : public Direct_rule<Transport_rule>
{
	private:

		Genode::Allocator       &_alloc;
		Permit_any_rule         *_permit_any_rule_ptr { };
		Permit_single_rule_tree  _permit_single_rules { };

		static Permit_any_rule *
		_read_permit_any_rule(Domain_dict            &domains,
		                      Genode::Xml_node const  node,
		                      Genode::Allocator      &alloc);

		/*
		 * Noncopyable
		 */
		Transport_rule(Transport_rule const &);
		Transport_rule &operator = (Transport_rule const &);

	public:

		Transport_rule(Ipv4_address_prefix const &dst,
		               Genode::Allocator         &alloc);

		~Transport_rule();

		void find_permit_rule_by_port(Port port, auto const &handle_match, auto const &handle_no_match) const
		{
			if (_permit_any_rule_ptr) {

				handle_match(*_permit_any_rule_ptr);

			} else {

				_permit_single_rules.find_by_port(
					port, handle_match, handle_no_match);
			}
		}

		[[nodiscard]] bool finish_construction(Domain_dict            &domains,
		                                       Genode::Xml_node const  node,
		                                       Genode::Cstring  const &protocol,
		                                       Configuration          &config,
		                                       Domain           const &domain);
};


class Net::Transport_rule_list : public Direct_rule_list<Transport_rule>
{
	public:

		void find_best_match(Ipv4_address const &ip,
		                     Port         const  port,
		                     auto         const &handle_match,
		                     auto         const &handle_no_match) const
		{
			find_longest_prefix_match(
				ip,
				[&] /* handle_match */ (Transport_rule const &transport_rule)
				{
					transport_rule.find_permit_rule_by_port(
						port,
						[&] /* handle_match */ (Permit_rule const &permit_rule)
						{
							handle_match(transport_rule, permit_rule);
						},
						handle_no_match);
				},
				handle_no_match
			);
		}
};

#endif /* _TRANSPORT_RULE_H_ */
