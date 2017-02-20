/*
 * \brief  Report session interface
 * \author Norman Feske
 * \date   2014-01-10
 *
 * A report session allows a client to report status information about itself
 * to the outer world, in particular to its parent process.
 *
 * The communication between client and server is based on the combination
 * of shared memory with synchronous RPC. A dataspace shared between both
 * processes is used to carry the data. RPC calls are used to synchronize the
 * access to the buffer. When the client performs an RPC, it hands over the
 * responsibility to access the buffer to the server. While an RPC is in
 * progress and the client blocks for the reply, the server may read and write
 * the buffer. At all other times, the server is not expected to access the
 * buffer.
 *
 * This hand over of the buffer between both parties is a mere convention. It
 * is not enforced by the system. For this reason, neither of both proceess
 * must keep its internal state stored in the buffer. Data should always be
 * copied in/out and never processed directly in the buffer.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REPORT_SESSION__REPORT_SESSION_H_
#define _INCLUDE__REPORT_SESSION__REPORT_SESSION_H_

/* Genode includes */
#include <session/session.h>
#include <base/rpc.h>
#include <base/signal.h>
#include <dataspace/capability.h>

namespace Report {

	using Genode::Dataspace_capability;
	using Genode::Signal_context_capability;
	using Genode::size_t;

	struct Session;
	struct Session_client;
}


struct Report::Session : Genode::Session
{
	static const char *service_name() { return "Report"; }

	typedef Session_client Client;

	/**
	 * Request the dataspace used to carry reports and responses
	 */
	virtual Dataspace_capability dataspace() = 0;

	/**
	 * Submit data that is currently contained in the dataspace as report
	 *
	 * \param length  length of report in bytes
	 *
	 * While this method is called, the information in the dataspace
	 * must not be modified by the client.
	 */
	virtual void submit(size_t length) = 0;

	/**
	 * Install signal handler for response notifications
	 */
	virtual void response_sigh(Signal_context_capability) = 0;

	/**
	 * Request a response from the recipient of reports
	 *
	 * By calling this method, the client expects that the server will
	 * replace the content of the dataspace with new information.
	 *
	 * \return length of response in bytes
	 */
	virtual size_t obtain_response() = 0;


	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_submit, void, submit, size_t);
	GENODE_RPC(Rpc_response_sigh, void, response_sigh, Signal_context_capability);
	GENODE_RPC(Rpc_obtain_response, size_t, obtain_response);
	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_submit, Rpc_response_sigh,
	                     Rpc_obtain_response);
};

#endif /* _INCLUDE__REPORT_SESSION__REPORT_SESSION_H_ */
