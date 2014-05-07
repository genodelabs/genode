/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

#include <platform_thread.h>

namespace Genode {

	class Platform_thread;
	class Platform_pd
	{
		public:

			/**
			 * Constructors
			 */
			Platform_pd(bool core);
			Platform_pd(char const *, signed pd_id = -1, bool create = true);

			/**
			 * Destructor
			 */
			~Platform_pd();

			/**
			 * Bind thread to protection domain
			 *
			 * \return  0  on success or
			 *         -1  if thread ID allocation failed.
			 */
			int bind_thread(Platform_thread *thread);

			/**
			 * Unbind thread from protection domain
			 *
			 * Free the thread's slot and update thread object.
			 */
			void unbind_thread(Platform_thread *thread);

			/**
			 * Assign parent interface to protection domain
			 */
			int assign_parent(Native_capability parent) { return 0; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
