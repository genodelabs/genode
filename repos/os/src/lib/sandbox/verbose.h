/*
 * \brief  Sandbox verbosity
 * \author Norman Feske
 * \date   2017-01-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__VERBOSE_H_
#define _LIB__SANDBOX__VERBOSE_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <util/xml_node.h>

/* local includes */
#include <types.h>

namespace Sandbox { struct Verbose; }


class Sandbox::Verbose : Genode::Noncopyable
{
	private:

		bool _enabled = false;

	public:

		Verbose() { }

		Verbose(Genode::Xml_node config)
		: _enabled(config.attribute_value("verbose", false)) { }

		bool enabled() const { return _enabled; }
};

#endif /* _LIB__SANDBOX__VERBOSE_H_ */
