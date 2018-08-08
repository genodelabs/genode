/*
 * \brief  Custom define of system/ulib/ddk/include/hw/inout.h
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INOUT_H_
#define _INOUT_H_

uint8_t inp(uint16_t);
void outp(uint16_t, uint8_t);

#endif
