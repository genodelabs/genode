/*
 * \brief  POSIX semaphore implementation
 * \author Christian Prochaska
 * \date   2012-03-12
 *
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/semaphore.h>
#include <semaphore.h>

using namespace Genode;

extern "C" {

	/*
	 * This class is named 'struct sem' because the 'sem_t' type is
	 * defined as 'struct sem*' in 'semaphore.h'
	 */
	struct sem : Semaphore
	{
		sem(int value) : Semaphore(value) { }
	};


	int sem_close(sem_t *)
	{
		warning(__func__, " not implemented");
		return -1;
	}


	int sem_destroy(sem_t *sem)
	{
		delete *sem;
		return 0;
	}


	int sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
	{
		*sval = (*sem)->cnt();
		return 0;
	}


	int sem_init(sem_t *sem, int pshared, unsigned int value)
	{
		*sem = new struct sem(value);
		return 0;
	}


	sem_t *sem_open(const char *, int, ...)
	{
		warning(__func__, " not implemented");
		return 0;
	}


	int sem_post(sem_t *sem)
	{
		(*sem)->up();
		return 0;
	}


	int sem_timedwait(sem_t * __restrict, const struct timespec * __restrict)
	{
		warning(__func__, " not implemented");
		return -1;
	}


	int sem_trywait(sem_t *)
	{
		warning(__func__, " not implemented");
		return -1;
	}


	int sem_unlink(const char *)
	{
		warning(__func__, " not implemented");
		return -1;
	}


	int sem_wait(sem_t *sem)
	{
		(*sem)->down();
		return 0;
	}

}
