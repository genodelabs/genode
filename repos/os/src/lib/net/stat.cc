/*
 * \brief  Parse ethernet packets and find magic values to start performance
 *         measurements
 * \author Alexander Boettcher 
 * \date   2013-03-26
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <nic/stat.h>

#include <net/ipv4.h>
#include <net/udp.h>

namespace Nic {

enum Measurement::status Measurement::_check(Net::Ethernet_frame * eth,
                                             Genode::size_t size)
{
	using namespace Net;

	if (Genode::memcmp(eth->dst().addr, _mac.addr, sizeof(_mac.addr)))
		return Measurement::UNKNOWN;

	Ipv4_packet *ip = new (eth->data<void>()) Ipv4_packet(size -
	                                                sizeof(Ethernet_frame));

	if (ip->protocol() != Ipv4_packet::Protocol::UDP)
		return Measurement::UNKNOWN;

	Udp_packet *udp = new (ip->data<void>()) Udp_packet(size - sizeof(Ethernet_frame)
	                                              - sizeof(Ipv4_packet));

	Genode::uint8_t magic [] = "Hello world! Genode is greeting.";

	if (Genode::memcmp(udp->data<void>(), magic, sizeof(magic) - 1))
		return Measurement::FOR_US;
	return Measurement::IS_MAGIC;
}

void Measurement::data(Net::Ethernet_frame * eth, Genode::size_t size)
{

	enum status status = _check(eth, size);

	if (status == UNKNOWN) {
		_drop.count ++;
		_drop.size += size;

		return;
	}

	_stat.count ++;
	_stat.size += size;

	if (status != IS_MAGIC)
		return;

	using namespace Genode;

	addr_t old = _timestamp;
	_timestamp  = _timer.elapsed_ms();

	unsigned long time_elapsed_ms = _timestamp - old;
	unsigned long kbits_test = 0;
	unsigned long kbits_raw  = 0;

	if (time_elapsed_ms)
		kbits_test = _stat.size / time_elapsed_ms * 8;
	if (time_elapsed_ms)
		kbits_raw  = (_stat.size + _drop.size) / time_elapsed_ms * 8;

	log(kbits_test, " kBit/s (raw ", kbits_raw, " kBit/s), "
	    "runtime ", time_elapsed_ms, " ms");
	log((unsigned long)(_stat.size / 1024), " kiBytes (+ ",
	    (unsigned long)(_drop.size / 1024), " kiBytes dropped)");
	log(_stat.count, " packets (+ ", _drop.count, " packets dropped)");
	log("");

	_stat.size = _stat.count = _drop.size = _drop.count = 0;

	_timestamp = _timer.elapsed_ms();
}

}
