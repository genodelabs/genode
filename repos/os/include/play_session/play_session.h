/*
 * \brief  Audio-play session interface
 * \author Norman Feske
 * \date   2023-12-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLAY_SESSION__PLAY_SESSION_H_
#define _INCLUDE__PLAY_SESSION__PLAY_SESSION_H_

#include <dataspace/capability.h>
#include <base/rpc_server.h>
#include <session/session.h>

namespace Play {

	using namespace Genode;

	struct Seq
	{
		static constexpr unsigned LIMIT = 1 << 7, MASK = LIMIT - 1;
		unsigned _value; /* 0...127 */
		unsigned value() const { return _value & MASK; }
	};

	struct Time_window { unsigned start, end; };

	struct Sample_start { unsigned index; };

	struct Num_samples
	{
		unsigned _value; /* 0...4095 */
		unsigned value() const { return _value & 0xfff; }
	};

	struct Duration
	{
		static constexpr unsigned LIMIT = 100*1000;
		unsigned us;
		bool valid() const { return us > 0 && us <= LIMIT; }
	};

	struct Session;
}


struct Play::Session : Genode::Session
{
	/**
	 * Layout of the audio buffer shared between client and server
	 */
	struct Shared_buffer
	{
		static const unsigned NUM_SLOTS   = 20;
		static const unsigned MAX_SAMPLES = 8*1024; /* 160 ms at 50 KHz */

		struct Slot
		{
			Seq          acquired_seq;
			Time_window  time_window;
			Sample_start sample_start;  /* offset within 'samples' */
			Num_samples  num_samples;
			Seq          committed_seq; /* detect slot modification during read */
		};

		Slot slots[NUM_SLOTS];

		float samples[MAX_SAMPLES]; /* ring buffer of sample values */
	};

	static constexpr size_t DATASPACE_SIZE = align_addr(sizeof(Shared_buffer), 12);

	/**
	 * \noapi
	 */
	static const char *service_name() { return "Play"; }

	/*
	 * A play session consumes a dataspace capability for the server's
	 * session-object allocation, a dataspace capability for the audio
	 * buffer, and its session capability.
	 */
	static constexpr unsigned CAP_QUOTA = 3;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_schedule, Time_window, schedule, Time_window, Duration, Num_samples);
	GENODE_RPC(Rpc_stop, void, stop);
	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_schedule, Rpc_stop);
};

#endif /* _INCLUDE__EVENT_SESSION__EVENT_SESSION_H_ */
