/*
 * \brief  Work queue implementation
 * \author Josef Soentgen
 * \author Stefan Kalkowski
 * \date   2015-10-26
 */

/*
 * Copyright (C) 2015-2106 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_KIT__WORK_H_
#define _LX_KIT__WORK_H_

#include <base/allocator.h>

namespace Lx {
	class Work;
}


class Lx::Work
{
	public:


		virtual ~Work() { }

		/**
		 * Unblock corresponding task
		 */
		virtual void unblock() = 0;

		/**
		 * Schedule work
		 */
		virtual void schedule(struct ::work_struct *) = 0;

		/**
		 * Schedule delayed work
		 */
		virtual void schedule_delayed(struct ::delayed_work *, unsigned long delay) = 0;

		/**
		 * Schedule delayed work
		 */
		virtual void schedule_tasklet(struct ::tasklet_struct *) = 0;

		/**
		 * Cancel work item
		 */
		virtual bool cancel_work(struct ::work_struct *, bool sync = false) = 0;

		/**
		 * Return task name
		 */
		virtual char const *task_name() = 0;

		static Work &work_queue(Genode::Allocator *alloc = nullptr);
		static Work *alloc_work_queue(Genode::Allocator *alloc, char const *name);
		static void  free_work_queue(Work *W);
};

#endif /* _LX_KIT__WORK_H_ */
