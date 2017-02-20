/*
 * \brief  Noux verbosity
 * \author Norman Feske
 * \date   2017-01-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__VERBOSE_H_
#define _NOUX__VERBOSE_H_

#include <util/noncopyable.h>
#include <util/xml_node.h>

namespace Noux { struct Verbose; }


class Noux::Verbose : Genode::Noncopyable
{
	private:

		bool const _enabled;
		bool const _ld;
		bool const _syscalls;
		bool const _quota;

	public:

		Verbose(Genode::Xml_node config)
		:
			_enabled (config.attribute_value("verbose",          false)),
			_ld      (config.attribute_value("ld_verbose",       false)),
			_syscalls(config.attribute_value("verbose_syscalls", false)),
			_quota   (config.attribute_value("verbose_quota",    false))
		{ }

		bool enabled()  const { return _enabled;  }
		bool ld()       const { return _ld;       }
		bool syscalls() const { return _syscalls; }
		bool quota()    const { return _quota;    }
};

#endif /* _NOUX__VERBOSE_H_ */
