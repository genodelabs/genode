/*
 * \brief  MAC-address allocator
 * \author Stefan Kalkowski
 * \date   2010-08-25
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <mac_allocator.h>

/**
 * We take the range 02:02:02:02:02:XX for our MAC address allocator,
 * it's likely, that we will have no clashes here.
 * (e.g. Linux uses 02:00... for its tap-devices.)
 */
Net::Mac_address Net::Mac_allocator::mac_addr_base(0x03);
