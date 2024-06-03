/*
 * \brief  State tracking for UDP/TCP connections
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/tcp.h>

/* local includes */
#include <link.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


/******************
 ** Link_side_id **
 ******************/

constexpr size_t Link_side_id::data_size()
{
	return sizeof(src_ip) + sizeof(src_port) +
	       sizeof(dst_ip) + sizeof(dst_port);
}


bool Link_side_id::operator != (Link_side_id const &id) const
{
	return memcmp(id.data_base(), data_base(), data_size()) != 0;
}


bool Link_side_id::operator > (Link_side_id const &id) const
{
	return memcmp(id.data_base(), data_base(), data_size()) > 0;
}


/***************
 ** Link_side **
 ***************/

Link_side::Link_side(Domain             &domain,
                     Link_side_id const &id,
                     Link               &link)
:
	_domain_ptr(&domain), _id(id), _link(link)
{
	if (link.config().verbose()) {
		log("[", domain, "] new ", l3_protocol_name(link.protocol()),
		    " link ", is_client() ? "client" : "server", ": ", *this);
	}
}


void Link_side::print(Output &output) const
{
	Genode::print(output, "src ", src_ip(), ":", src_port(),
	                     " dst ", dst_ip(), ":", dst_port());
}


bool Link_side::is_client() const
{
	return this == &_link.client();
}


/**********
 ** Link **
 **********/

void Link::print(Output &output) const
{
	Genode::print(output, "CLN ", _client, " SRV ", _server);
}


Link::Link(Interface                     &cln_interface,
           Domain                        &cln_domain,
           Link_side_id            const &cln_id,
           Port_allocator_guard          *srv_port_alloc_ptr,
           Domain                        &srv_domain,
           Link_side_id            const &srv_id,
           Cached_timer                  &timer,
           Configuration                 &config,
           L3_protocol             const  protocol,
           Microseconds            const  dissolve_timeout,
           Interface_link_stats          &stats)
:
	_config_ptr(&config),
	_client_interface(cln_interface),
	_server_port_alloc_ptr(srv_port_alloc_ptr),
	_dissolve_timeout(timer, *this, &Link::_handle_dissolve_timeout,
	                  Microseconds { 100 * 1000 }),
	_dissolve_timeout_us(dissolve_timeout),
	_protocol(protocol),
	_client(cln_domain, cln_id, *this),
	_server(srv_domain, srv_id, *this),
	_stats(stats),
	_stats_ptr(&stats.opening)
{
	(*_stats_ptr)++;
	_client_interface.links(_protocol).insert(this);
	_client.domain().links(_protocol).insert(&_client);
	_server.domain().links(_protocol).insert(&_server);
	_dissolve_timeout.schedule(_dissolve_timeout_us);
}


Link::~Link() { _stats.destroyed++; }


void Link::_handle_dissolve_timeout(Duration)
{
	dissolve(true);
	_client_interface.links(_protocol).remove(this);
	_client_interface.dissolved_links(_protocol).insert(this);
}


void Link::dissolve(bool timeout)
{

	(*_stats_ptr)--;
	if (timeout) {
		if (_stats_ptr == &_stats.opening) { _stats_ptr = &_stats.dissolved_timeout_opening; }
		if (_stats_ptr == &_stats.open)    { _stats_ptr = &_stats.dissolved_timeout_open; }
		if (_stats_ptr == &_stats.closing) { _stats_ptr = &_stats.dissolved_timeout_closing; }
		if (_stats_ptr == &_stats.closed)  { _stats_ptr = &_stats.dissolved_timeout_closed; }
	} else {
		_stats_ptr = &_stats.dissolved_no_timeout;
	}
	(*_stats_ptr)++;

	_client.domain().links(_protocol).remove(&_client);
	_server.domain().links(_protocol).remove(&_server);
	if (_config_ptr->verbose()) {
		log("Dissolve ", l3_protocol_name(_protocol), " link: ", *this); }

	if (_server_port_alloc_ptr) {
		if (_config_ptr->verbose()) {
			log("Free ", l3_protocol_name(_protocol),
			    " port ", _server.dst_port(),
			    " at ", _server.domain(),
			    " that was used by ", _client.domain());
		}
		_server_port_alloc_ptr->free(_server.dst_port());
	}
}


void Link::handle_config(Domain               &cln_domain,
                         Domain               &srv_domain,
                         Port_allocator_guard *srv_port_alloc_ptr,
                         Configuration        &config)
{
	Microseconds dissolve_timeout_us(0);
	switch (_protocol) {
	case L3_protocol::TCP:  dissolve_timeout_us = config.tcp_idle_timeout();  break;
	case L3_protocol::UDP:  dissolve_timeout_us = config.udp_idle_timeout();  break;
	case L3_protocol::ICMP: dissolve_timeout_us = config.icmp_idle_timeout(); break;
	default: ASSERT_NEVER_REACHED;
	}
	_dissolve_timeout_us = dissolve_timeout_us;
	_dissolve_timeout.schedule(_dissolve_timeout_us);

	_client.domain().links(_protocol).remove(&_client);
	_server.domain().links(_protocol).remove(&_server);

	_config_ptr = &config;
	_client._domain_ptr = &cln_domain;
	_server._domain_ptr = &srv_domain;
	_server_port_alloc_ptr = srv_port_alloc_ptr;

	cln_domain.links(_protocol).insert(&_client);
	srv_domain.links(_protocol).insert(&_server);

	if (config.verbose()) {
		log("[", cln_domain, "] update link client: ", _client);
		log("[", srv_domain, "] update link server: ", _server);
	}
}


/**************
 ** Tcp_link **
 **************/

Tcp_link::Tcp_link(Interface                  &cln_interface,
                   Domain                     &cln_domain,
                   Link_side_id         const &cln_id,
                   Port_allocator_guard       *srv_port_alloc_ptr,
                   Domain                     &srv_domain,
                   Link_side_id         const &srv_id,
                   Cached_timer               &timer,
                   Configuration              &config,
                   L3_protocol          const  protocol,
                   Interface_link_stats       &stats,
                   Tcp_packet                 &tcp)
:
	Link(cln_interface, cln_domain, cln_id, srv_port_alloc_ptr, srv_domain, srv_id, timer,
	     config, protocol, config.tcp_idle_timeout(), stats)
{
	client_packet(tcp);
}


void Tcp_link::_closing()
{
	_state = State::CLOSING;
	(*_stats_ptr)--;
	_stats_ptr = &_stats.closing;
	(*_stats_ptr)++;
}


void Tcp_link::_closed()
{
	_state = State::CLOSED;
	(*_stats_ptr)--;
	_stats_ptr = &_stats.closed;
	(*_stats_ptr)++;
}


void Tcp_link::_tcp_packet(Tcp_packet &tcp,
                           Peer       &sender,
                           Peer       &receiver)
{
	if (_state == State::OPENING)
		_opening_tcp_packet(tcp, sender, receiver);

	if (_state == State::CLOSED) {
		return; }

	if (tcp.rst()) {
		_closed();
	} else {
		if (tcp.fin()) {
			sender.fin = true;
			_closing();
		}
		if (receiver.fin && tcp.ack()) {
			receiver.fin_acked = true;
			if (sender.fin_acked) {
				_closed(); }
			else {
				_closing();
			}
		}
	}
	switch (_state) {
	case State::OPENING:
	case State::OPEN: _packet(); break;
	case State::CLOSED: _dissolve_timeout.schedule(Microseconds(0UL)); break;
	default: _dissolve_timeout.schedule(Microseconds(_config_ptr->tcp_max_segm_lifetime().value << 1)); break;
	}
}


void Tcp_link::_opening_tcp_packet(Tcp_packet const &tcp, Peer &sender, Peer &receiver)
{
	if (tcp.syn())
		sender.syn = true;
	if (tcp.ack() && receiver.syn && !receiver.syn_acked)
		receiver.syn_acked = true;
	if (sender.syn_acked && receiver.syn_acked) {
		_opening = false;
		_state = State::OPEN;
		(*_stats_ptr)--;
		if (_stats_ptr == &_stats.opening) { _stats_ptr = &_stats.open; }
		(*_stats_ptr)++;
	}
}


/**************
 ** Udp_link **
 **************/

Udp_link::Udp_link(Interface                  &cln_interface,
                   Domain                     &cln_domain,
                   Link_side_id         const &cln_id,
                   Port_allocator_guard       *srv_port_alloc_ptr,
                   Domain                     &srv_domain,
                   Link_side_id         const &srv_id,
                   Cached_timer               &timer,
                   Configuration              &config,
                   L3_protocol          const  protocol,
                   Interface_link_stats       &stats)
:
	Link(cln_interface, cln_domain, cln_id, srv_port_alloc_ptr, srv_domain, srv_id, timer,
	     config, protocol, config.udp_idle_timeout(), stats)
{ }


void Udp_link::server_packet()
{
	if (_opening) {
		_opening = false;
		(*_stats_ptr)--;
		if (_stats_ptr == &_stats.opening) { _stats_ptr = &_stats.open; }
		(*_stats_ptr)++;
	}
	_packet();
}


/***************
 ** Icmp_link **
 ***************/

Icmp_link::Icmp_link(Interface                  &cln_interface,
                     Domain                     &cln_domain,
                     Link_side_id         const &cln_id,
                     Port_allocator_guard       *srv_port_alloc_ptr,
                     Domain                     &srv_domain,
                     Link_side_id         const &srv_id,
                     Cached_timer               &timer,
                     Configuration              &config,
                     L3_protocol          const  protocol,
                     Interface_link_stats       &stats)
:
	Link(cln_interface, cln_domain, cln_id, srv_port_alloc_ptr, srv_domain, srv_id, timer,
	     config, protocol, config.icmp_idle_timeout(), stats)
{ }


void Icmp_link::server_packet()
{
	if (_opening) {
		_opening = false;
		(*_stats_ptr)--;
		if (_stats_ptr == &_stats.opening)
			_stats_ptr = &_stats.open;
		(*_stats_ptr)++;
	}
	_packet();
}
