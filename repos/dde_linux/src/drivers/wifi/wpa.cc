/*
 * \brief  Wpa_supplicant thread of the wifi driver
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2019-12-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/blockade.h>
#include <base/env.h>
#include <base/sleep.h>

/* libc includes */
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#include "wpa.h"


/* entry function */
extern "C" int wpa_main(void);


void * Wpa_thread::_entry_trampoline(void *arg)
{
	Wpa_thread *t = (Wpa_thread *)arg;
	t->_entry();
	return nullptr;
}


void Wpa_thread::_entry()
{
	/* wait until the wifi driver is up and running */
	_blockade.block();
	_exit = wpa_main();
	Genode::sleep_forever();
}


Wpa_thread::Wpa_thread(Genode::Env &env, Genode::Blockade &blockade)
: _blockade(blockade), _exit(-1)
{
	pthread_t tid = 0;
	if (pthread_create(&tid, 0, _entry_trampoline, this) != 0) {
		printf("Error: pthread_create() failed\n");
		exit(-1);
	}
}
