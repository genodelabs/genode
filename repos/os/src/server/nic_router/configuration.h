/*
 * \brief  Reflects the current router configuration through objects
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

/* local includes */
#include <domain.h>

/* Genode includes */
#include <os/duration.h>

namespace Genode { class Allocator; }

namespace Net { class Configuration; }


class Net::Configuration
{
	private:

		Genode::Allocator          &_alloc;
		bool                 const  _verbose;
		Genode::Microseconds const  _rtt;
		Domain_tree                 _domains;
		Genode::Xml_node     const  _node;

		Genode::Microseconds _init_rtt(Genode::Xml_node const node);

	public:

		enum { DEFAULT_RTT_SEC = 6 };

		Configuration(Genode::Xml_node const node, Genode::Allocator &alloc);


		/***************
		 ** Accessors **
		 ***************/

		bool                  verbose() const { return _verbose; }
		Genode::Microseconds  rtt()     const { return _rtt; }
		Domain_tree          &domains()       { return _domains; }
		Genode::Xml_node      node()    const { return _node; }
};

#endif /* _CONFIGURATION_H_ */
