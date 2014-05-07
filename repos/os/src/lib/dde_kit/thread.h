/*
 * \brief  Thread facility
 * \author Christian Helmuth
 * \date   2008-10-20
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <util/avl_tree.h>

#ifndef _THREAD_H_
#define _THREAD_H_

namespace Dde_kit {

	using namespace Genode;

	typedef Genode::Thread<0x2000> Thread;

	/*
	 * The thread information is splitted from the actual (runable) thread, so
	 * that information for adopted threads can be managed.
	 */
	class Thread_info : public Avl_node<Thread_info>
	{
		public:

			class Not_found : public Exception { };

		private:

			Thread_base *_thread_base;
			const char  *_name;
			unsigned     _id;
			void        *_data;

			static unsigned _id_counter;

		public:

			Thread_info(Thread_base *thread_base, const char *name);
			~Thread_info();

			/** AVL node comparison */
			bool higher(Thread_info *info);

			/** AVL node lookup */
			Thread_info *lookup(Thread_base *thread_base);

			/***************
			 ** Accessors **
			 ***************/

			Thread_base *thread_base()  { return _thread_base; }
			const char *name() const    { return _name; }
			void name(const char *name) { _name = name; }
			unsigned id() const         { return _id; }
			void *data() const          { return _data; }
			void data(void *data)       { _data = data; }
	};
}

#endif /* _THREAD_H_ */
