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
#include <interface.h>
#include <configuration.h>
#include <protocol_name.h>

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


bool Link_side_id::operator == (Link_side_id const &id) const
{
	return memcmp(id.data_base(), data_base(), data_size()) == 0;
}


bool Link_side_id::operator > (Link_side_id const &id) const
{
	return memcmp(id.data_base(), data_base(), data_size()) > 0;
}


/***************
 ** Link_side **
 ***************/

Link_side::Link_side(Interface          &interface,
                     Link_side_id const &id,
                     Link               &link)
:
	_interface(interface), _id(id), _link(link)
{ }


Link_side const &Link_side::find_by_id(Link_side_id const &id) const
{
	if (id == _id) {
		return *this; }

	bool const side = id > _id;
	Link_side *const link_side = Avl_node<Link_side>::child(side);
	if (!link_side) {
		throw Link_side_tree::No_match(); }

	return link_side->find_by_id(id);
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


/********************
 ** Link_side_tree **
 ********************/

Link_side const &Link_side_tree::find_by_id(Link_side_id const &id) const
{
	Link_side *const link_side = first();
	if (!link_side) {
		throw No_match(); }

	return link_side->find_by_id(id);
}


/**********
 ** Link **
 **********/

void Link::print(Output &output) const
{
	Genode::print(output, "CLN ", _client, " SRV ", _server);
}


Link::Link(Interface                           &cln_interface,
           Link_side_id                  const &cln_id,
           Pointer<Port_allocator_guard> const  srv_port_alloc,
           Interface                           &srv_interface,
           Link_side_id                  const &srv_id,
           Timer::Connection                   &timer,
           Configuration                       &config,
           uint8_t                       const  protocol)
:
	_config(config),
	_client(cln_interface, cln_id, *this),
	_server_port_alloc(srv_port_alloc),
	_server(srv_interface, srv_id, *this),
	_close_timeout(timer, *this, &Link::_handle_close_timeout),
	_close_timeout_us(_config.rtt_sec() * 2 * 1000 * 1000),
	_protocol(protocol)
{
	_close_timeout.schedule(_close_timeout_us);
}


void Link::_handle_close_timeout(Duration)
{
	dissolve();
	_client._interface.link_closed(*this, _protocol);
}


void Link::dissolve()
{
	_client._interface.dissolve_link(_client, _protocol);
	_server._interface.dissolve_link(_server, _protocol);
	if (_config.verbose()) {
		log("Dissolve ", protocol_name(_protocol), " link: ", *this); }

	try {
		_server_port_alloc.deref().free(_server.dst_port());
		if (_config.verbose()) {
			log("Free ", protocol_name(_protocol),
			    " port ", _server.dst_port(),
			    " at ", _server.interface(),
			    " that was used by ", _client.interface());
		}
	}
	catch (Pointer<Port_allocator_guard>::Invalid) { }
}


/**************
 ** Tcp_link **
 **************/

Tcp_link::Tcp_link(Interface                           &cln_interface,
                   Link_side_id                  const &cln_id,
                   Pointer<Port_allocator_guard> const  srv_port_alloc,
                   Interface                           &srv_interface,
                   Link_side_id                  const &srv_id,
                   Timer::Connection                   &timer,
                   Configuration                       &config,
                   uint8_t                       const  protocol)
:
	Link(cln_interface, cln_id, srv_port_alloc, srv_interface, srv_id, timer,
	     config, protocol)
{ }


void Tcp_link::_fin_acked()
{
	if (_server_fin_acked && _client_fin_acked) {
		_close_timeout.schedule(_close_timeout_us);
		_closed = true;
	}
}


void Tcp_link::server_packet(Tcp_packet &tcp)
{
	if (_closed) {
		return; }

	if (tcp.fin()) {
		_server_fin  = true; }

	if (tcp.ack() && _client_fin) {
		_client_fin_acked = true;
		_fin_acked();
	}
	if (!_closed) {
		_packet(); }
}


void Tcp_link::client_packet(Tcp_packet &tcp)
{
	if (_closed) {
		return; }

	if (tcp.fin()) {
		_client_fin = true; }

	if (tcp.ack() && _server_fin) {
		_server_fin_acked = true;
		_fin_acked();
	}
	if (!_closed) {
		_packet(); }
}


/**************
 ** Udp_link **
 **************/

Udp_link::Udp_link(Interface                           &cln_interface,
                   Link_side_id                  const &cln_id,
                   Pointer<Port_allocator_guard> const  srv_port_alloc,
                   Interface                           &srv_interface,
                   Link_side_id                  const &srv_id,
                   Timer::Connection                   &timer,
                   Configuration                       &config,
                   uint8_t                       const  protocol)
:
	Link(cln_interface, cln_id, srv_port_alloc, srv_interface, srv_id, timer,
	     config, protocol)
{ }
