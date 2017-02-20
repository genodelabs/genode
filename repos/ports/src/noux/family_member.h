/*
 * \brief  Helper for handling the relationship between Noux processes
 * \author Norman Feske
 * \date   2012-02-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__FAMILY_MEMBER_H_
#define _NOUX__FAMILY_MEMBER_H_

/* Genode includes */
#include <util/list.h>
#include <base/lock.h>

/* Noux includes */
#include <parent_exit.h>
#include <parent_execve.h>

namespace Noux { class Family_member; }


class Noux::Family_member : public List<Family_member>::Element,
                            public Parent_exit,
                            public Parent_execve
{
	private:

		int           const _pid;
		Lock                _lock;
		List<Family_member> _list;
		bool                _has_exited;
		int                 _exit_status;

	protected:

		/**
		 * Lock used for implementing blocking syscalls,
		 * i.e., select, wait4, ...
		 */
		Lock _blocker;

	public:

		Family_member(int pid)
		: _pid(pid), _has_exited(false), _exit_status(0)
		{ }

		virtual ~Family_member() { }

		int pid() const { return _pid; }

		int exit_status() const { return _exit_status; }

		/**
		 * Called by the parent at creation time of the process
		 */
		void insert(Family_member *member)
		{
			Lock::Guard guard(_lock);
			_list.insert(member);
		}

		/**
		 * Called by the parent from the return path of the wait4 syscall
		 */
		void remove(Family_member *member)
		{
			Lock::Guard guard(_lock);
			_list.remove(member);
		}

		virtual void submit_signal(Noux::Sysio::Signal sig) = 0;

		/**
		 * Called by the parent (originates from Kill_broadcaster)
		 */
		bool deliver_kill(int pid, Noux::Sysio::Signal sig)
		{
			Lock::Guard guard(_lock);

			if (pid == _pid) {
				submit_signal(sig);
				return true;
			}

			bool result = false;

			for (Family_member *child = _list.first(); child; child = child->next())
				if (child->deliver_kill(pid, sig))
					result = true;

			return result;
		}

		/**
		  * Parent_exit interface
		  */

		/* Called by the child on the parent (via Parent_exit) */
		void exit_child()
		{
			submit_signal(Sysio::Signal::SIG_CHLD);
		}

		/**
		 * Parent_execve interface
		 */

		/* Called by the parent from 'execve_child()' */
		virtual Family_member *do_execve(const char *filename,
		                                 Args const &args,
		                                 Sysio::Env const &env) = 0;

		/* Called by the child on the parent (via Parent_execve) */
		void execve_child(Family_member &child,
		                  const char *filename,
		                  Args const &args,
		                  Sysio::Env const &env)
		{
			Lock::Guard guard(_lock);
			Family_member *new_child = child.do_execve(filename,
			                                           args,
			                                           env);
			_list.insert(new_child);
			_list.remove(&child);
		}


		/**
		 * Tell the parent that we exited
		 */
		void exit(int exit_status)
		{
			_exit_status = exit_status;
			_has_exited  = true;
		}

		Family_member *poll4()
		{
			Lock::Guard guard(_lock);

			/* check if any of our children has exited */
			Family_member *curr = _list.first();
			for (; curr; curr = curr->next()) {
				if (curr->_has_exited)
					return curr;
			}
			return 0;
		}

		/**
		 * Wait for the exit of any of our children
		 */
		Family_member *wait4()
		{
			/* reset the blocker lock to the 'locked' state */
			_blocker.unlock();
			_blocker.lock();

			Family_member *result = poll4();
			if (result)
				return result;

			_blocker.lock();

			/* either a child exited or a signal occurred */
			return poll4();
		}
};

#endif /* _NOUX__FAMILY_MEMBER_H_ */
