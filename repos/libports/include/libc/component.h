/*
 * \brief  Hook functions for bootstrapping a libc-using Genode component
 * \author Norman Feske
 * \date   2016-12-23
 *
 * This interface is implemented by components that use both Genode's API and
 * the libc. For such components, the libc provides the 'Component::construct'
 * function that takes the precautions needed for letting the application use
 * blocking I/O via POSIX functions like 'read' or 'select'. The libc's
 * 'Component::construct' function finally passes control to the application by
 * calling the application-provided 'Libc::Component::construct' function.
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LIBC__COMPONENT_H_
#define _INCLUDE__LIBC__COMPONENT_H_

#include <base/env.h>
#include <base/stdint.h>

namespace Genode { struct Env; }


/**
 * Interface to be provided by the component implementation
 */
namespace Libc { namespace Component {

	/**
	 * Return stack size of the component's initial entrypoint
	 */
	Genode::size_t stack_size();

	/**
	 * Construct component
	 *
	 * \param env  interface to the component's execution environment
	 */
	void construct(Genode::Env &env);
} }

#endif /* _INCLUDE__LIBC__COMPONENT_H_ */
