/*
 * \brief  Internet protocol version 4
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <net/ipv4.h>

const Net::Ipv4_packet::Ipv4_address Net::Ipv4_packet::CURRENT(0x00);
const Net::Ipv4_packet::Ipv4_address Net::Ipv4_packet::BROADCAST(0xFF);
