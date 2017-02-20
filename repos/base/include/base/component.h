/*
 * \brief  Hook functions for bootstrapping the component
 * \author Norman Feske
 * \date   2015-12-17
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__COMPONENT_H_
#define _INCLUDE__BASE__COMPONENT_H_

#include <base/env.h>
#include <base/stdint.h>

namespace Genode { struct Env; }


/**
 * Interface to be provided by the component implementation
 */
namespace Component
{
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
}

#endif /* _INCLUDE__BASE__COMPONENT_H_ */
