/*
 * \brief  DDE iPXE dummy implementations
 * \author Christian Helmuth
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "local.h"

int snprintf(char *buf, __SIZE_TYPE__ size, const char *fmt, ...) { TRACE; return 0; }

void clear_settings() { TRACE; }
void netdev_settings_operations() { TRACE; }
void dbg_autocolourise(unsigned long id) { }
void dbg_decolourise() { }
void strerror() { TRACE; }

/* for eepro100.c */
void init_spi_bit_basher() { TRACE; }
void nvs_read() { TRACE; }
void threewire_detect_address_len() { TRACE; }
void threewire_read() { TRACE; }
void threewire_write() { TRACE; }
