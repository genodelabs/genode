/*
 * \brief  ROM session implementation used by Noux processes for initial ROMs
 * \author Christian Prochaska
 * \date   2017-01-31
 *
 * The initial ROMs (binary and linker) are already attached in a forked
 * child and don't need a new ROM dataspace. The invalid dataspace returned
 * by this component is handled in 'Child::Process'.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__EMPTY_ROM_SESSION_COMPONENT_H_
#define _NOUX__EMPTY_ROM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>

namespace Noux { class Empty_rom_session_component; }

class Noux::Empty_rom_session_component : public Rpc_object<Rom_session>
{
	private:

		Rpc_entrypoint &_ep;

	public:

		Empty_rom_session_component(Rpc_entrypoint &ep)
		: _ep(ep)
		{
			_ep.manage(this);
		}

		~Empty_rom_session_component()
		{
			_ep.dissolve(this);
		}


		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace()
		{
			return Rom_dataspace_capability();
		}

		void sigh(Signal_context_capability) { }
};

#endif /* _NOUX__EMPTY_ROM_SESSION_COMPONENT_H_ */
