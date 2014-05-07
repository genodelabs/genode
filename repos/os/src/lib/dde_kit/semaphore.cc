/*
 * \brief  Semaphores
 * \author Christian Helmuth
 * \date   2008-09-15
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <base/semaphore.h>

extern "C" {
#include <dde_kit/semaphore.h>
}

using namespace Genode;


struct dde_kit_sem : public Semaphore
{
	public:

		dde_kit_sem(int value) : Semaphore(value) { }
};


extern "C" void dde_kit_sem_down(dde_kit_sem_t *sem) {
	sem->down(); }


extern "C" int dde_kit_sem_down_try(dde_kit_sem_t *sem)
{
	PERR("not implemented - will potentially block");
	sem->down();

	/* success */
	return 0;
}


extern "C" void dde_kit_sem_up(dde_kit_sem_t *sem) { sem->up(); }


extern "C" dde_kit_sem_t *dde_kit_sem_init(int value)
{
	dde_kit_sem *s;

	try {
		s = new (env()->heap()) dde_kit_sem(value);
	} catch (...) {
		PERR("allocation failed (size=%zd)", sizeof(*s));
		return 0;
	}

	return s;
}


extern "C" void dde_kit_sem_deinit(dde_kit_sem_t *sem) {
	destroy(env()->heap(), sem); }
