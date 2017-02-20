/*
 * \brief  DDE iPXE dummy implementations
 * \author Christian Helmuth
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <dde_support.h>

#define TRACE dde_printf("\033[35m%s not implemented\033[0m\n", __func__)

int snprintf(char *buf, __SIZE_TYPE__ size, const char *fmt, ...) { TRACE; return 0; }

void clear_settings()             { TRACE; }
void netdev_settings_operations() { TRACE; }

/* for drivers/net/realtek.c */
void nvo_init()       { TRACE; }
void register_nvo()   { TRACE; }
void unregister_nvo() { TRACE; }
