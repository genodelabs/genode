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

	class  Configuration;
	class  Transport_rule;
	struct Transport_rule_list : Direct_rule_list<Transport_rule> { };
}


class Net::Transport_rule : public Direct_rule<Transport_rule>
{
	private:

		Genode::Allocator              &_alloc;
		Pointer<Permit_any_rule> const  _permit_any_rule;
		Permit_single_rule_tree         _permit_single_rules { };

		static Pointer<Permit_any_rule>
		_read_permit_any_rule(Domain_tree            &domains,
		                      Genode::Xml_node const  node,
		                      Genode::Allocator      &alloc);

	public:

		Transport_rule(Domain_tree            &domains,
		               Genode::Xml_node const  node,
		               Genode::Allocator      &alloc,
		               Genode::Cstring  const &protocol,
		               Configuration          &config);

		~Transport_rule();

		Permit_rule const &permit_rule(Port const port) const;
};

#endif /* _TRANSPORT_RULE_H_ */
