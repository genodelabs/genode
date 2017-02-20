/*
 * \brief  Synchronized access to Vancouver motherboard
 * \author Norman Feske
 * \date   2013-05-16
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SYNCED_MOTHERBOARD_H_
#define _SYNCED_MOTHERBOARD_H_

#include <nul/motherboard.h>
#include <base/synced_interface.h>

typedef Genode::Synced_interface<Motherboard> Synced_motherboard;

#endif /* _SYNCED_MOTHERBOARD_H_ */
