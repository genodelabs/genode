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
#include <packet_log.h>
#include <util/xml_node.h>

using namespace Net;
using namespace Genode;


void Net::Interface::_handle_eth(void              *const  eth_base,
                                 size_t             const  eth_size,
                                 Packet_descriptor  const &)
{
	try {
		Ethernet_frame &eth = *reinterpret_cast<Ethernet_frame *>(eth_base);
		Interface &remote = _remote.deref();

		if (_log_time) {
			Genode::Duration const new_time    = _timer.curr_time();
			uint64_t         const new_time_ms = new_time.trunc_to_plain_us().value / 1000;
			uint64_t         const old_time_ms = _curr_time.trunc_to_plain_us().value / 1000;

			log("\033[33m(", remote._label, " <- ", _label, ")\033[0m ",
			    packet_log(eth, _log_cfg), " \033[33mtime ", new_time_ms,
			    " ms (Δ ", new_time_ms - old_time_ms, " ms)\033[0m");

			_curr_time = new_time;
		} else {
			log("\033[33m(", remote._label, " <- ", _label, ")\033[0m ", 
			    packet_log(eth, _log_cfg));
		}
		remote._send(eth, eth_size);
	}
	catch (Pointer<Interface>::Invalid) {
		error("no remote interface set"); }
}


void Net::Interface::_send(Ethernet_frame &eth, Genode::size_t const size)
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


void Net::Interface::_ready_to_submit()
{
	while (_sink().packet_avail()) {

		Packet_descriptor const pkt = _sink().get_packet();
		if (!pkt.size() || !_sink().packet_valid(pkt)) {
			continue; }

		_handle_eth(_sink().packet_content(pkt), pkt.size(), pkt);

		if (!_sink().ready_to_ack()) {
			error("ack state FULL");
			return;
		}
		_sink().acknowledge_packet(pkt);
	}
}


void Net::Interface::_ready_to_ack()
{
	while (_source().ack_avail()) {
		_source().release_packet(_source().get_acked_packet()); }
}


Net::Interface::Interface(Entrypoint        &ep,
                          Interface_label    label,
                          Timer::Connection &timer,
                          Duration          &curr_time,
                          bool               log_time,
                          Xml_node           config)
:
	_sink_ack          { ep, *this, &Interface::_ack_avail },
	_sink_submit       { ep, *this, &Interface::_ready_to_submit },
	_source_ack        { ep, *this, &Interface::_ready_to_ack },
	_source_submit     { ep, *this, &Interface::_packet_avail },
	_label             { label },
	_timer             { timer },
	_curr_time         { curr_time },
	_log_time          { log_time },
	_default_log_style { config.attribute_value("default", Packet_log_style::DEFAULT) },
	_log_cfg           { config.attribute_value("eth",     _default_log_style),
	                     config.attribute_value("arp",     _default_log_style),
	                     config.attribute_value("ipv4",    _default_log_style),
	                     config.attribute_value("dhcp",    _default_log_style),
	                     config.attribute_value("udp",     _default_log_style),
	                     config.attribute_value("icmp",    _default_log_style),
	                     config.attribute_value("tcp",     _default_log_style) }
{ }
