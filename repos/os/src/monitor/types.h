/*
 * \brief  Common types used within monitor component
 * \author Norman Feske
 * \date   2023-05-08
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <base/id_space.h>
#include <base/allocator.h>
#include <base/entrypoint.h>
#include <base/sleep.h>
#include <session/session.h>

namespace Monitor {

	using namespace Genode;

	template <typename> struct Monitored_rpc_object;

	constexpr static uint16_t GDB_PACKET_MAX_SIZE = 16*1024;

	struct Inferior_pd;

	using Inferiors = Id_space<Inferior_pd>;

	struct Monitored_thread;

	using Threads = Id_space<Monitored_thread>;

	static inline void never_called(char const *) __attribute__((noreturn));
	static inline void never_called(char const *method_name)
	{
		error("unexpected call of ", method_name);
		sleep_forever();
	}

	/**
	 * Call 'monitored_fn' with local RPC object that belongs to 'cap', or
	 * 'direct_fn' if 'cap' does not belong to any local RPC object of type
	 * 'OBJ'.
	 */
	template <typename OBJ>
	static void with_monitored(Entrypoint &ep, Capability<typename OBJ::Interface> cap,
	                           auto const &monitored_fn, auto const &direct_fn)
	{
		OBJ *monitored_obj_ptr = nullptr;
		ep.rpc_ep().apply(cap, [&] (OBJ *ptr) {
			monitored_obj_ptr = ptr; });

		if (monitored_obj_ptr)
			monitored_fn(*monitored_obj_ptr);
		else
			direct_fn();
	}
}


template <typename IF>
struct Monitor::Monitored_rpc_object : Rpc_object<IF>
{
	Entrypoint &_ep;

	using Name = String<Session::Label::capacity()>;

	Name const _name;

	using Interface = IF;

	Capability<IF> _real;

	Monitored_rpc_object(Entrypoint &ep, Capability<IF> real, Name const &name)
	:
		_ep(ep), _name(name), _real(real)
	{
		_ep.manage(*this);
	}

	~Monitored_rpc_object() { _ep.dissolve(*this); }
};

#endif /* _TYPES_H_ */
