/*
 * \brief  Zircon thread wrapper
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ZX_THREAD_H_
#define _ZX_THREAD_H_

#include <base/thread.h>

#include <threads.h>

namespace ZX
{
	class Thread;
}

class ZX::Thread : public Genode::Thread
{
	private:

		thrd_start_t _thread_worker;
		unsigned long _arg;
		int _result;

		void entry() override;

	public:

		Thread(Genode::Env &, thrd_start_t, const char *, void *);
		int result();
};

#endif /* ifndef _ZX_THREAD_H_ */
