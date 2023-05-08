/*
 * \brief  Utilities for generating responses for the GDB protocol
 * \author Norman Feske
 * \date   2023-05-12
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GDB_RESPONSE_H_
#define _GDB_RESPONSE_H_

#include <monitor/string.h>
#include <monitor/output.h>

namespace Genode {

	void gdb_response(Output &, auto const &fn);

	static inline void gdb_ok   (Output &);
	static inline void gdb_error(Output &, uint8_t);
}


/**
 * Calls 'fn' with an output interface that wraps the date into a GDB packet
 */
void Genode::gdb_response(Output &output, auto const &fn)
{
	Gdb_checksummed_output checksummed_output { output };

	fn(checksummed_output);
};


/**
 * Generate OK response
 */
static inline void Genode::gdb_ok(Output &output)
{
	gdb_response(output, [&] (Output &out) { print(out, "OK"); });
}


/**
 * Generate error respones
 */
static inline void Genode::gdb_error(Output &output, uint8_t errno)
{
	gdb_response(output, [&] (Output &out) { print(out, "E", Gdb_hex(errno)); });
}

#endif /* _GDB_RESPONSE_H_ */
