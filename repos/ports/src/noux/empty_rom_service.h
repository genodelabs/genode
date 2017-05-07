/*
 * \brief  ROM service provided to Noux processes for initial ROMs
 * \author Christian Prochaska
 * \date   2017-01-31
 *
 * The initial ROMs (binary and linker) are already attached in a forked
 * child and don't need a new ROM dataspace.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__EMPTY_ROM_SERVICE_H_
#define _NOUX__EMPTY_ROM_SERVICE_H_

/* Genode includes */
#include <base/service.h>

/* Noux includes */
#include <empty_rom_session_component.h>

namespace Noux {

	typedef Local_service<Empty_rom_session_component> Empty_rom_service;
	class Empty_rom_factory;
}


class Noux::Empty_rom_factory : public Empty_rom_service::Factory
{
	private:

		Allocator      &_alloc;
		Rpc_entrypoint &_ep;

	public:

		Empty_rom_factory(Allocator &alloc, Rpc_entrypoint &ep)
		: _alloc(alloc), _ep(ep) { }

		Empty_rom_session_component &create(Args const &args, Affinity) override
		{
			try {
				return *new (_alloc) Empty_rom_session_component(_ep); }
			catch (Rom_connection::Rom_connection_failed) {
				throw Service_denied(); }
		}

		void upgrade(Empty_rom_session_component &, Args const &) override { }

		void destroy(Empty_rom_session_component &session) override
		{
			Genode::destroy(_alloc, &session);
		}
};

#endif /* _NOUX__EMPTY_ROM_SERVICE_H_ */
