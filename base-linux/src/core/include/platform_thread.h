/*
 * \brief  Linux thread facility
 * \author Norman Feske
 * \date   2006-06-13
 *
 * Pretty dumb.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LINUX__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__LINUX__PLATFORM_THREAD_H_

#include <base/pager.h>
#include <base/thread_state.h>

namespace Genode {

	class Platform_thread
	{
		private:

			unsigned long _tid;
			unsigned long _pid;
			char          _name[32];

		public:

			/**
			 * Constructor
			 */
			Platform_thread(const char *name, unsigned priority, addr_t);

			/**
			 * Cancel currently blocking operation
			 */
			void cancel_blocking();

			/**
			 * Pause this thread
			 */
			void pause();

			/**
			 * Resume this thread
			 */
			void resume();

			/**
			 * Dummy implementation of platform-thread interface
			 */
			Pager_object *pager() { return 0; }
			void          pager(Pager_object *) { }
			int           start(void *ip, void *sp) { return 0; }
			int           state(Thread_state *state_dst) { return 0; }
			const char   *name() { return _name; }
			void          affinity(unsigned) { }
	};
}

#endif /* _CORE__INCLUDE__LINUX__PLATFORM_THREAD_H_ */
