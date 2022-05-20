/*
 * \brief  Stream id 0
 * \author Johannes Schlatow
 * \date   2021-08-04
 *
 * Defines CTF events for our CTF stream with id 0. This must match the
 * metadata file.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRACE_RECORDER_POLICY__CTF_STREAM0_H_
#define _TRACE_RECORDER_POLICY__CTF_STREAM0_H_

#include <base/fixed_stdint.h>

#include <ctf/events/named.h>
#include <trace_recorder_policy/ctf.h>

namespace Ctf {
	struct Rpc_call;
	struct Rpc_returned;
	struct Rpc_dispatch;
	struct Rpc_reply;
	struct Signal_submit;
	struct Signal_receive;
	struct Checkpoint;
}


struct Ctf::Rpc_call : Trace_recorder::Ctf_event,
                       Ctf::Named_event
{
	Rpc_call(const char *name, size_t len)
	: Trace_recorder::Ctf_event(1),
	  Named_event(name, len)
	{ }
} __attribute__((packed));


struct Ctf::Rpc_returned : Trace_recorder::Ctf_event,
                           Ctf::Named_event
{
	Rpc_returned(const char *name, size_t len)
	: Trace_recorder::Ctf_event(2),
	  Named_event(name, len)
	{ }
} __attribute__((packed));


struct Ctf::Rpc_dispatch : Trace_recorder::Ctf_event,
                           Ctf::Named_event
{
	Rpc_dispatch(const char *name, size_t len)
	: Trace_recorder::Ctf_event(3),
	  Named_event(name, len)
	{ }
} __attribute__((packed));


struct Ctf::Rpc_reply : Trace_recorder::Ctf_event,
                        Ctf::Named_event
{
	Rpc_reply(const char *name, size_t len)
	: Trace_recorder::Ctf_event(4),
	  Named_event(name, len)
	{ }
} __attribute__((packed));


struct Ctf::Signal_submit  : Trace_recorder::Ctf_event
{
	uint32_t _number;

	Signal_submit(uint32_t number)
	: Trace_recorder::Ctf_event(5),
	  _number(number)
	{ }
} __attribute__((packed));


struct Ctf::Signal_receive : Trace_recorder::Ctf_event
{
	uint32_t _number;
	uint64_t _context;

	Signal_receive(uint32_t number, void* context)
	: Trace_recorder::Ctf_event(6),
	  _number(number),
	  _context((uint64_t)context)
	{ }
} __attribute__((packed));


struct Ctf::Checkpoint : Trace_recorder::Ctf_event
{
	uint32_t     _data;
	uint64_t     _addr;
	uint8_t      _type;
	Named_event  _named;

	Checkpoint(const char *name, size_t len, uint32_t data, void* addr, uint8_t type)
	: Trace_recorder::Ctf_event(7),
	  _data(data),
	  _addr((uint64_t)addr),
	  _type(type),
	  _named(name, len)
	{ }
} __attribute__((packed));

#endif /* _TRACE_RECORDER_POLICY__CTF_STREAM0_H_ */
