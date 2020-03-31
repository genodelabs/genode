/*
 * \brief  Policy module function declarations
 * \author Josef Soentgen
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <base/stdint.h>

typedef Genode::size_t size_t;

namespace Genode {
	struct Msgbuf_base;
	struct Signal_context;
}

extern "C" size_t max_event_size ();
extern "C" size_t log_output     (char *dst, char const *log_message, size_t len);
extern "C" size_t rpc_call       (char *dst, char const *rpc_name, Genode::Msgbuf_base const &);
extern "C" size_t rpc_returned   (char *dst, char const *rpc_name, Genode::Msgbuf_base const &);
extern "C" size_t rpc_dispatch   (char *dst, char const *rpc_name);
extern "C" size_t rpc_reply      (char *dst, char const *rpc_name);
extern "C" size_t signal_submit  (char *dst, unsigned const);
extern "C" size_t signal_receive (char *dst, Genode::Signal_context const &, unsigned);
