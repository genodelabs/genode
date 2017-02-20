/*
 * \brief  Init verbosity
 * \author Norman Feske
 * \date   2017-01-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__INIT__VERBOSE_H_
#define _INCLUDE__INIT__VERBOSE_H_

#include <util/noncopyable.h>
#include <util/xml_node.h>

namespace Init { struct Verbose; }


class Init::Verbose : Genode::Noncopyable
{
	private:

		bool _enabled;

	public:

		Verbose(Genode::Xml_node config)
		: _enabled(config.attribute_value("verbose", false)) { }

		bool enabled() const { return _enabled; }
};

#endif /* _INCLUDE__INIT__VERBOSE_H_ */
