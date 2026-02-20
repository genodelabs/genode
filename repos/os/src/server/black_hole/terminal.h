/*
 * \brief  Terminal session component and root
 * \author Sebastian Sumpf
 * \date   2026-02-20
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL_H_
#define _TERMINAL_H_

#include <base/attached_ram_dataspace.h>
#include <terminal_session/terminal_session.h>
#include <root/component.h>

namespace Black_hole {

	using namespace Genode;

	class Terminal_session;
	class Terminal_root;
}


class Black_hole::Terminal_session : public Session_object<Terminal::Session, Terminal_session>
{
	private:

		Env                    &_env;
		Attached_ram_dataspace _ram_ds;

	public:

		static constexpr size_t DATASPACE_SIZE = 0x1000;

		Terminal_session(Env &env, Resources const &resources, Label const &label)
		:
			Session_object(env.ep(), resources, label), _env(env),
			_ram_ds(_env.ram(), _env.rm(), DATASPACE_SIZE)
		{ }

		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size()  override { return Size(0, 0); }
		bool avail() override { return false; }

		void read_avail_sigh(Signal_context_capability)   override { }
		void size_changed_sigh(Signal_context_capability) override { }

		void connected_sigh(Signal_context_capability sigh) override {
			Signal_transmitter(sigh).submit(); }

		size_t read(void *, size_t)        override { return 0; }
		size_t write(void const *, size_t) override { return 0; }

		/*******************
		 ** RPC interface **
		 *******************/

		Dataspace_capability _dataspace() { return _ram_ds.cap(); }

		size_t _read(size_t) { return 0; }
		size_t _write(size_t num_bytes) { return num_bytes; }
};


class Black_hole::Terminal_root : public Root_component<Terminal_session>
{
	private:

		Env &_env;

	protected:

		Create_result _create_session(const char *args) override
		{
			if (session_resources_from_args(args).ram_quota.value < Terminal_session::DATASPACE_SIZE)
				throw Insufficient_ram_quota();

			return *new (md_alloc())
				Terminal_session(_env,
				                 session_resources_from_args(args),
				                 session_label_from_args(args));
		}

	public:

		Terminal_root(Env &env, Allocator &md_alloc)
		:
			Root_component<Terminal_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }
};

#endif /* _TERMINAL_H_ */

