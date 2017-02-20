/*
 * \brief  Client-side Report session interface
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REPORT_SESSION__CLIENT_H_
#define _INCLUDE__REPORT_SESSION__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <report_session/report_session.h>

namespace Report { struct Session_client; }


struct Report::Session_client : Genode::Rpc_client<Session>
{
	Session_client(Genode::Capability<Session> cap)
	: Genode::Rpc_client<Session>(cap) { }

	Dataspace_capability dataspace() override {
		return call<Rpc_dataspace>(); }

	void submit(size_t length) override {
		call<Rpc_submit>(length); }

	void response_sigh(Signal_context_capability cap) override {
		call<Rpc_response_sigh>(cap); }

	size_t obtain_response() override {
		return call<Rpc_obtain_response>(); }
};

#endif /* _INCLUDE__REPORT_SESSION__CLIENT_H_ */
