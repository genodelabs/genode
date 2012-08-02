/*
 * \brief  Protection-domain facility
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

#include <platform_thread.h>

/*
 * Must be initialized by the startup code,
 * only valid in core
 */
extern Genode::addr_t __core_pd_sel;

namespace Genode {

	class Platform_thread;
	class Platform_pd
	{
		private:

			Native_capability _parent;
			int               _thread_cnt;
			addr_t            _pd_sel;

		public:

			/**
			 * Constructors
			 */
			Platform_pd(signed pd_id = -1, bool create = true);

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
			int assign_parent(Native_capability parent);

			/**
			 * Return portal capability selector for parent interface
			 */
			addr_t parent_pt_sel() { return _parent.local_name(); }

			/**
			 * Assign PD selector to PD
			 */
			void assign_pd(addr_t pd_sel) { _pd_sel = pd_sel; }

			/**
			 * Capability selector of this task.
			 *
			 * \return PD selector
			 */
			addr_t pd_sel() { return _pd_sel; }

			/**
			 * Capability selector of core protection domain
			 *
			 * \return PD selector
			 */
			static addr_t pd_core_sel() { return __core_pd_sel; }

	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
