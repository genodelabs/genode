/*
 * \brief  Core-specific instance of the ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_

#include <base/rpc_server.h>
#include <dataspace_component.h>
#include <rom_session/rom_session.h>

/* core includes */
#include <rom_fs.h>

namespace Core { struct Rom_session_component; }


struct Core::Rom_session_component : Rpc_object<Rom_session>
{
	struct Ds
	{
		Rpc_entrypoint     &_ep;
		Dataspace_component _ds;

		static Capability<Rom_dataspace> _rom_ds_cap(Capability<Dataspace> cap)
		{
			/*
			 * On Linux, the downcast from 'Linux_dataspace' to 'Dataspace'
			 * happens implicitly by passing the argument. The upcast to
			 * 'Linux_dataspace' happens explicitly.
			 */
			return static_cap_cast<Rom_dataspace>(cap);
		}

		Capability<Rom_dataspace> const cap = _rom_ds_cap(_ep.manage(&_ds));

		Ds(Rpc_entrypoint &ep, auto &&... args) : _ep(ep), _ds(args...) { }

		~Ds() { _ep.dissolve(&_ds); }
	};

	Constructible<Ds> _ds { };

	/**
	 * Constructor
	 *
	 * \param rom_fs  ROM filesystem
	 * \param ep      entry point to manage the dataspace
	 *                corresponding the rom session
	 * \param args    session-construction arguments
	 */
	Rom_session_component(Rom_fs &rom_fs, Rpc_entrypoint &ep, const char *args);


	/***************************
	 ** Rom session interface **
	 ***************************/

	Rom_dataspace_capability dataspace() override
	{
		return _ds.constructed() ? _ds->cap : Capability<Rom_dataspace>();
	}

	void sigh(Signal_context_capability) override { }
};

#endif /* _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_ */
