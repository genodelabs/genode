/**
 * \brief   Startup code for core on ARM
 * \author  Stefan Kalkowski
 * \date    2015-03-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/**************************
 ** .text (program code) **
 **************************/

.section ".text"

	/* program entry-point */
	.global _core_start
	_core_start:

	/* create proper environment for main thread */
	bl init_main_thread

	/* apply environment that was created by init_main_thread */
	ldr sp, =init_main_thread_result
	ldr sp, [sp]

	/* jump into init C code instead of calling it as it should never return */
	b _main
