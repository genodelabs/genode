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
#include <pointer.h>

namespace Genode { class Allocator; }

namespace Net {

	class Configuration;
	class Transport_rule;
	class Transport_rule_list;
}


class Net::Transport_rule : public Direct_rule<Transport_rule>
{
	private:

		Genode::Allocator              &_alloc;
		Pointer<Permit_any_rule> const  _permit_any_rule;
		Permit_single_rule_tree         _permit_single_rules { };

		static Pointer<Permit_any_rule>
		_read_permit_any_rule(Domain_dict            &domains,
		                      Genode::Xml_node const  node,
		                      Genode::Allocator      &alloc);

	public:

		Transport_rule(Domain_dict            &domains,
		               Genode::Xml_node const  node,
		               Genode::Allocator      &alloc,
		               Genode::Cstring  const &protocol,
		               Configuration          &config,
		               Domain           const &domain);

		~Transport_rule();

		template <typename HANDLE_MATCH_FN,
		          typename HANDLE_NO_MATCH_FN>
		void
		find_permit_rule_by_port(Port            const port,
		                         HANDLE_MATCH_FN    && handle_match,
		                         HANDLE_NO_MATCH_FN && handle_no_match) const
		{
			if (_permit_any_rule.valid()) {

				handle_match(_permit_any_rule());

			} else {

				_permit_single_rules.find_by_port(
					port, handle_match, handle_no_match);
			}
		}
};


class Net::Transport_rule_list : public Direct_rule_list<Transport_rule>
{
	public:

		template <typename HANDLE_MATCH_FN,
		          typename HANDLE_NO_MATCH_FN>

		void find_best_match(Ipv4_address    const &ip,
		                     Port            const port,
		                     HANDLE_MATCH_FN    && handle_match,
		                     HANDLE_NO_MATCH_FN && handle_no_match) const
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
