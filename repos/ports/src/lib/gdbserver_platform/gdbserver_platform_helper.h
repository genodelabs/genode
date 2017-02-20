/*
 * \brief  Genode backend for GDBServer - helper functions
 * \author Christian Prochaska
 * \date   2011-07-07
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef GDBSERVER_PLATFORM_HELPER_H
#define GDBSERVER_PLATFORM_HELPER_H

/**
 * \throw Cpu_session::State_access_failed
 */
Genode::Thread_state get_current_thread_state();

void set_current_thread_state(Genode::Thread_state thread_state);

void fetch_register(const char *reg_name,
                           Genode::addr_t thread_state_reg,
                           unsigned long &value);

void cannot_fetch_register(const char *reg_name);

bool store_register(const char *reg_name,
                    Genode::addr_t &thread_state_reg,
                    unsigned long value);

void cannot_store_register(const char *reg_name, unsigned long value);

#endif /* GDBSERVER_PLATFORM_HELPER_H */
