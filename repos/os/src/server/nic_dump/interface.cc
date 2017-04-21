/*
 * \brief  A net interface in form of a signal-driven NIC-packet handler
 * \author Martin Stein
 * \date   2017-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <interface.h>

/* Genode includes */
#include <net/ethernet.h>

using namespace Net;
using namespace Genode;


void Interface::_handle_eth(void              *const  eth_base,
                            size_t             const  eth_size,
                            Packet_descriptor  const &pkt)
{
	try {
		Ethernet_frame &eth = *new (eth_base) Ethernet_frame(eth_size);
		Interface &remote = _remote.deref();
		unsigned new_time = _timer.curr_time().trunc_to_plain_us().value / 1000;
		if (_log_time) {
			log("\033[33m(", remote._label, " <- ", _label, ")\033[0m ", eth,
			    " \033[33mtime ", new_time, " (", new_time - _curr_time,
			    ")\033[0m");
		} else {
			log("\033[33m(", remote._label, " <- ", _label, ")\033[0m ", eth);
		}
		_curr_time = new_time;
		remote._send(eth, eth_size);
	}
	catch (Ethernet_frame::No_ethernet_frame) {
		error("invalid ethernet frame"); }

	catch (Pointer<Interface>::Invalid) {
		error("no remote interface set"); }
}


void Interface::_send(Ethernet_frame &eth, Genode::size_t const size)
{
	try {
		Packet_descriptor const pkt = _source().alloc_packet(size);
		char *content = _source().packet_content(pkt);
		Genode::memcpy((void *)content, (void *)&eth, size);
		_source().submit_packet(pkt);
	}
	catch (Packet_stream_source::Packet_alloc_failed) {
		error("Failed to allocate packet"); }
}


void Interface::_ready_to_submit()
{
	while (_sink().packet_avail()) {

		Packet_descriptor const pkt = _sink().get_packet();
		if (!pkt.size()) {
			continue; }

		_handle_eth(_sink().packet_content(pkt), pkt.size(), pkt);

		if (!_sink().ready_to_ack()) {
			error("ack state FULL");
			return;
		}
		_sink().acknowledge_packet(pkt);
	}
}


void Interface::_ready_to_ack()
{
	while (_source().ack_avail()) {
		_source().release_packet(_source().get_acked_packet()); }
}


Interface::Interface(Entrypoint        &ep,
                     Interface_label    label,
                     Timer::Connection &timer,
                     unsigned          &curr_time,
                     bool               log_time,
                     Allocator         &alloc)
:
	_sink_ack     (ep, *this, &Interface::_ack_avail),
	_sink_submit  (ep, *this, &Interface::_ready_to_submit),
	_source_ack   (ep, *this, &Interface::_ready_to_ack),
	_source_submit(ep, *this, &Interface::_packet_avail),
	_alloc(alloc), _label(label), _timer(timer), _curr_time(curr_time),
	_log_time(log_time)
{ }
