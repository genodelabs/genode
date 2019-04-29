/*
 * \brief  Genode backend for GDBServer
 * \author Christian Prochaska
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * Based on gdbserver/linux-low.h
 */

#ifndef GENODE_LOW_H
#define GENODE_LOW_H

#include <sys/types.h>

#include "server.h"

/* exception type */
struct No_memory_at_address { };

/* interface for linux-low.c */

void genode_stop_all_threads();
void genode_continue_thread(unsigned long lwpid, int single_step);

int genode_kill(int pid);
int genode_detach(int pid);
void genode_fetch_registers(struct regcache *regcache, int regno);
void genode_store_registers(struct regcache *regcache, int regno);
int genode_read_memory(CORE_ADDR memaddr, unsigned char *myaddr, int len);
int genode_write_memory (CORE_ADDR memaddr, const unsigned char *myaddr, int len);

/* interface for genode-low.cc and low.cc */

int genode_fetch_register(int regno, unsigned long *reg_content);
void genode_store_register(int regno, unsigned long reg_content);
unsigned char genode_read_memory_byte(void *addr);

/* interface for cpu_thread_component.cc */
void genode_set_initial_breakpoint_at(unsigned long addr);

#endif /* GENODE_LOW_H */
