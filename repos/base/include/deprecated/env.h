/*
 * \brief  Environment of a component
 * \author Norman Feske
 * \date   2006-07-01
 *
 * \deprecated  This interface will be removed once all components are
 *              adjusted to the new API of base/component.h
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DEPRECATED__ENV_H_
#define _INCLUDE__DEPRECATED__ENV_H_

#include <parent/capability.h>
#include <parent/parent.h>
#include <region_map/region_map.h>
#include <cpu_session/cpu_session.h>
#include <cpu_session/capability.h>
#include <pd_session/capability.h>
#include <base/allocator.h>

namespace Genode {

	struct Env_deprecated;

	/**
	 * Return the interface to the component's environment
	 *
	 * \noapi
	 * \deprecated
	 */
	extern Env_deprecated *env_deprecated();

	/**
	 * Return the interface to the component's environment
	 *
	 * \deprecated
	 */
	static inline Env_deprecated *env() __attribute__((deprecated));
	static inline Env_deprecated *env()
	{
		return env_deprecated();
	}
}


/**
 * Component runtime environment
 *
 * The environment of a Genode component is defined by its parent. The 'Env'
 * class allows the component to interact with its environment. It is
 * initialized at the startup of the component.
 */
struct Genode::Env_deprecated : Interface
{
	/**
	 * Communication channel to our parent
	 */
	virtual Parent *parent() = 0;

	/**
	 * CPU session of the component
	 *
	 * This session is used to create the threads of the component.
	 */
	virtual Cpu_session *cpu_session() = 0;
	virtual Cpu_session_capability cpu_session_cap() = 0;

	/**
	 * Region-manager session of the component as created by the parent
	 *
	 * \deprecated  This function exists for API compatibility only.
	 *              The functionality of the former RM service is now
	 *              provided by the 'Region_map' interface.
	 */
	virtual Region_map *rm_session() = 0;

	/**
	 * PD session of the component as created by the parent
	 */
	virtual Pd_session *pd_session() = 0;
	virtual Pd_session_capability pd_session_cap() = 0;
};

#endif /* _INCLUDE__DEPRECATED__ENV_H_ */
