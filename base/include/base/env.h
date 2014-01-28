/*
 * \brief  Environment of a process
 * \author Norman Feske
 * \date   2006-07-01
 *
 * The environment of a Genode process is defined by its parent and initialized
 * on startup.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__ENV_H_
#define _INCLUDE__BASE__ENV_H_

#include <parent/capability.h>
#include <parent/parent.h>
#include <rm_session/rm_session.h>
#include <ram_session/ram_session.h>
#include <cpu_session/cpu_session.h>
#include <cpu_session/capability.h>
#include <pd_session/pd_session.h>
#include <base/allocator.h>
#include <base/snprintf.h>
#include <base/lock.h>

namespace Genode {

	class Env
	{
		public:

			virtual ~Env() { }

			/**
			 * Communication channel to our parent
			 */
			virtual Parent *parent() = 0;

			/**
			 * RAM session for the program
			 *
			 * The RAM Session represents a quota of memory that is
			 * available to the program. Quota can be used to allocate
			 * RAM-Dataspaces.
			 */
			virtual Ram_session *ram_session() = 0;
			virtual Ram_session_capability ram_session_cap() = 0;

			/**
			 * CPU session for the program
			 *
			 * This session is used to create threads.
			 */
			virtual Cpu_session *cpu_session() = 0;
			virtual Cpu_session_capability cpu_session_cap() = 0;

			/**
			 * Region manager session of the program
			 */
			virtual Rm_session *rm_session() = 0;

			/**
			 * Pd session of the program
			 */
			virtual Pd_session *pd_session() = 0;

			/**
			 * Heap backed by the ram_session of the environment.
			 */
			virtual Allocator *heap() = 0;
	};

	extern Env *env();

	/**
	 * Return parent capability
	 *
	 * Platforms have to implement this function for environment
	 * initialization.
	 */
	Parent_capability parent_cap();
}

#endif /* _INCLUDE__BASE__ENV_H_ */
