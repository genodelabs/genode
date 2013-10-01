/*
 * \brief  Genode backend for GDBServer
 * \author Christian Prochaska
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * Based on gdbserver/linux-low.h
 */

#ifndef GENODE_LOW_H
#define GENODE_LOW_H

#include "server.h"

int genode_signal_fd();

void genode_wait_for_target_main_thread();
void genode_detect_all_threads();

void genode_stop_all_threads();
void genode_resume_all_threads();

ptid_t genode_wait_for_signal_or_gdb_interrupt(struct target_waitstatus *status);
void genode_continue_thread(unsigned long lwpid, int single_step);

int genode_fetch_register(int regno, unsigned long *reg_content);
void genode_store_register(int regno, unsigned long reg_content);
unsigned char genode_read_memory_byte(void *addr);
void genode_write_memory_byte(void *addr, unsigned char value);

int genode_detach(int pid);
int genode_kill(int pid);

#endif /* GENODE_LOW_H */
