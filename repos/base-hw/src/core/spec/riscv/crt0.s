/**
 * \brief  Startup code for the core's userland part
 * \author Sebastian Sumpf
 * \date   2015-09-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.section ".text"

.global _core_start
_core_start:

	/* create environment for main thread */
	jal init_main_thread

	/* load stack pointer from init_main_thread_result */
	la sp, init_main_thread_result
	ld sp, (sp)

	/* jump into init C-code */
	j _main
