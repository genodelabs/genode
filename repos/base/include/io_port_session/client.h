/*
 * \brief  Client-side I/O-port session interface
 * \author Christian Helmuth
 * \date   2007-04-17
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IO_PORT_SESSION__CLIENT_H_
#define _INCLUDE__IO_PORT_SESSION__CLIENT_H_

#include <io_port_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Io_port_session_client; }


struct Genode::Io_port_session_client : Rpc_client<Io_port_session>
{
	explicit Io_port_session_client(Io_port_session_capability session)
	: Rpc_client<Io_port_session>(session) { }

	unsigned char inb(unsigned short address) override {
		return call<Rpc_inb>(address); }

	unsigned short inw(unsigned short address) override {
		return call<Rpc_inw>(address); }

	unsigned inl(unsigned short address) override {
		return call<Rpc_inl>(address); }

	void outb(unsigned short address, unsigned char value) override {
		call<Rpc_outb>(address, value); }

	void outw(unsigned short address, unsigned short value) override {
		call<Rpc_outw>(address, value); }

	void outl(unsigned short address, unsigned value) override {
		call<Rpc_outl>(address, value); }
};

#endif /* _INCLUDE__IO_PORT_SESSION__CLIENT_H_ */
