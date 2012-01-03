/*
 * \brief  Syscall event handling behaviors
 * \author Martin Stein
 * \date   2010-11-18
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INCLUDE__GENERIC__SYSCALL_EVENTS_H_
#define _KERNEL__INCLUDE__GENERIC__SYSCALL_EVENTS_H_

#include <kernel/types.h>
#include <generic/event.h>

namespace Kernel {

	enum { SYSCALL_EVENT__ERROR   = 1,
	       SYSCALL_EVENT__WARNING = 1,
	       SYSCALL_EVENT__VERBOSE = 0 };


	class Thread;


	class Syscall_event : public Event { };


	class Print_char : public Syscall_event {

		protected:

			virtual bool _permission_to_do_print_char() = 0;

		public:

			typedef On_occurence__result On_print_char__result;

			On_print_char__result on_print_char(char c);
	};


	class Thread_create : public Syscall_event {

		protected:

			virtual bool _permission_to_do_thread_create()=0;

			void _on_thread_create__warning__failed()
			{
				if (SYSCALL_EVENT__WARNING)
					printf("Warning in Kernel::Thread_create::on_thread_create, syscall failed\n");
			}

			void _on_thread_create__verbose__success(Thread *t);

		public:

			struct Argument
			{
				Thread_id     tid;
				Protection_id pid;
				Thread_id     pager_tid;
				Utcb*         utcb;
				addr_t          vip;
				addr_t          vsp;
				bool          is_privileged;
			};

			typedef Thread_create_types::Result Result;
			typedef On_occurence__result        On_thread_create__result;

			On_thread_create__result on_thread_create(Argument *a, Result *r);
	};


	class Thread_kill : public Syscall_event
	{
		protected:

			virtual bool _permission_to_do_thread_kill() = 0;

			void _on_thread_kill__warning__failed()
			{
				if (SYSCALL_EVENT__WARNING)
					printf("Warning in Kernel::Thread_kill::on_thread_kill, syscall failed\n");
			}

			void _on_thread_kill__verbose__success(Thread_id tid);

		public:

			struct Argument { Thread_id tid; };

			typedef Thread_kill_types::Result Result;
			typedef On_occurence__result      On_thread_kill__result;

			On_thread_kill__result on_thread_kill(Argument *a, Result *r);
	};


	class Thread_sleep : public Syscall_event
	{
		protected:

			void _on_thread_sleep__verbose__success();

		public:

			typedef On_occurence__result On_thread_sleep__result;

			On_thread_sleep__result on_thread_sleep();
	};


	class Thread_wake : public Syscall_event
	{
		protected:

			virtual bool _permission_to_do_thread_wake(Thread *t) = 0;

			void _on_thread_wake__warning__failed()
			{
				if (SYSCALL_EVENT__WARNING)
					printf("Warning in Kernel::Thread_wake::on_thread_wake, syscall failed\n");
			}

			void _on_thread_wake__verbose__success(Thread_id tid);

		public:

			struct Argument { Thread_id tid; };

			typedef Thread_wake_types::Result Result;
			typedef On_occurence__result      On_thread_wake__result;

			On_thread_wake__result on_thread_wake(Argument* a, Result* r);
	};


	class Tlb_load : public Syscall_event
	{
		protected:

			virtual bool _permission_to_do_tlb_load() = 0;

		public:

			void on_tlb_load(Paging::Resolution* r);
	};


	class Thread_pager : public Syscall_event {

		protected:

			virtual bool _permission_to_do_thread_pager(Thread_id tid) = 0;

		public:

			void on_thread_pager(Thread_id target_tid, Thread_id pager_tid);
	};


	class Tlb_flush : public Syscall_event {

		protected:

			virtual bool _permission_to_do_tlb_flush() = 0;

		public:

			void on_tlb_flush(Paging::Virtual_page *first_page, unsigned size);
	};


	class Thread_yield: public Syscall_event
	{
		public:

			virtual void yield()=0;
	};
}

#endif /* _KERNEL__INCLUDE__GENERIC__SYSCALL_EVENTS_H_ */

