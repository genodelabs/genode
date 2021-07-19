/*
 * \brief  Task represents a cooperatively scheduled thread of control
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__TASK_H_
#define _LX_KIT__TASK_H_

#include <util/list.h>
#include <util/string.h>
#include <lx_kit/arch_execute.h>

namespace Lx_kit {
	class Scheduler;
	class Task;

	using namespace Genode;
}

class Lx_kit::Task : public Genode::List<Lx_kit::Task>::Element
{
	public:

		using Name = String<64>;

		enum State { INIT, RUNNING, BLOCKED };
		enum Type  { NORMAL, IRQ_HANDLER, TIME_HANDLER };

	private:

		Task(Task const &);
		Task &operator = (Task const &);

		State         _state    { INIT };
		int           _priority { 120  }; /* initial value of swapper task  */
		Type          _type;
		Scheduler   & _scheduler;
		void * const  _lx_task;        /* pointer of Linux task struct      */
		int const     _pid;            /* Linux process identifier          */
		Name          _name;           /* name of task                      */
		void        * _stack;          /* stack pointer                     */
		jmp_buf       _env;            /* execution state                   */
		jmp_buf       _saved_env;      /* saved state of thread calling run */
		int        (* _func) (void *); /* function to call                  */
		void        * _arg;            /* argument for function             */

	public:

		Task(int        (* func) (void*),
		     void        * arg,
		     void        * task,
		     int           pid,
		     char const  * name,
		     Scheduler   & scheduler,
		     Type          type);

		~Task();

		State  state()    const;
		Type   type()     const;
		int    priority() const;
		Name   name()     const;
		void * lx_task()  const;
		int    pid()      const;

		void block();
		void unblock();

		void priority(int prio);
		void name(const char * name);

		bool runnable() const;

		/**
		 * Run task until next preemption point
		 */
		void run();

		/**
		 * Request scheduling (of other tasks)
		 *
		 * Note, this task may not be blocked when calling schedule() depending
		 * on the use case.
		 */
		void schedule();

		/**
		 * Shortcut to enter blocking state and request scheduling
		 */
		void block_and_schedule();
};

#endif /* _LX_KIT__TASK_H_ */
