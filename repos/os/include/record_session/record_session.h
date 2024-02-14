/*
 * \brief  Audio-record session interface
 * \author Norman Feske
 * \date   2023-12-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RECORD_SESSION__RECORD_SESSION_H_
#define _INCLUDE__RECORD_SESSION__RECORD_SESSION_H_

#include <dataspace/capability.h>
#include <base/rpc_server.h>
#include <session/session.h>

namespace Record {

	using namespace Genode;

	/*
	 * The 'Time_window' values are merely used as tokens between 'record'
	 * and 'record_at' calls. They are not meant to be interpreted by the
	 * client.
	 */
	struct Time_window { unsigned start, end; };

	struct Num_samples
	{
		unsigned _value; /* 0...8191 (13 bits) */
		unsigned value() const { return _value & 0x1fff; }
	};

	struct Session;
}


struct Record::Session : Genode::Session
{
	/*
	 * The dataspace shared between client and server can hold 160 ms of 50 KHz
	 * audio, using one float (4 bytes) per sample.
	 */
	static constexpr size_t DATASPACE_SIZE = 32*1024;

	/**
	 * \noapi
	 */
	static const char *service_name() { return "Record"; }

	/*
	 * A record session consumes a dataspace capability for the server's
	 * session-object allocation, a dataspace capability for the audio
	 * buffer, and its session capability.
	 */
	static constexpr unsigned CAP_QUOTA = 3;

	struct Depleted { };

	using Record_result = Attempt<Time_window, Depleted>;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_wakeup_sigh, void, wakeup_sigh, Signal_context_capability);
	GENODE_RPC(Rpc_record, Record_result, record, Num_samples);
	GENODE_RPC(Rpc_record_at, void, record_at, Time_window, Num_samples);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_wakeup_sigh, Rpc_record, Rpc_record_at);
};

#endif /* _INCLUDE__EVENT_SESSION__EVENT_SESSION_H_ */
