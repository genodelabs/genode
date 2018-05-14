/*
 * \brief  Thread implementation for LwIP threads.
 * \author Stefan Kalkowski
 * \date   2009-11-14
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__INCLUDE__THREAD_H__
#define __LWIP__INCLUDE__THREAD_H__

#include <base/thread.h>

extern "C" {
#include <lwip/sys.h>
}

namespace Lwip {

	class Lwip_thread : public Genode::Thread_deprecated<16384>
	{
		private:

			void (*_thread)(void *arg);
			void  *_arg;

		public:

			Lwip_thread(const char *name,
			            void (* thread)(void *arg),
			            void *arg)
			: Genode::Thread_deprecated<16384>(name), _thread(thread), _arg(arg) { }

			void entry() { _thread(_arg); }
	};
}

#endif //__LWIP__INCLUDE__THREAD_H__
