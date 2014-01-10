/*
 * \brief  ROM dataspace utility
 * \author Norman Feske
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__ATTACHED_ROM_DATASPACE_H_
#define _INCLUDE__OS__ATTACHED_ROM_DATASPACE_H_

#include <util/volatile_object.h>
#include <os/attached_dataspace.h>
#include <rom_session/connection.h>

namespace Genode { class Attached_rom_dataspace; }


class Genode::Attached_rom_dataspace
{
	private:

		Rom_connection _rom;

		/*
		 * A ROM module may change or disappear over the lifetime of a ROM
		 * session. In contrast to the plain 'Attached_dataspace', which is
		 * always be valid once constructed, a 'Attached_rom_dataspace' has
		 * to handle the validity of the dataspace.
		 */
		Lazy_volatile_object<Attached_dataspace> _ds;

		/**
		 * Try to attach the ROM module, ignore invalid dataspaces
		 */
		void _try_attach()
		{
			try { _ds.construct(_rom.dataspace()); }
			catch (Attached_dataspace::Invalid_dataspace) { }
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw Rom_connection::Rom_connection_failed
		 * \throw Rm_session::Attach_failed
		 */
		Attached_rom_dataspace(char const *name)
		: _rom(name) { _try_attach(); }

		template <typename T> T *local_addr() { return _ds->local_addr<T>(); }

		size_t size() const { return _ds->size(); }

		/**
		 * Register signal handler for ROM module changes
		 */
		void sigh(Signal_context_capability sigh) { _rom.sigh(sigh); }

		/**
		 * Re-attach ROM module
		 */
		void update() { _try_attach(); }

		/**
		 * Return true of content is present
		 */
		bool is_valid() const { return _ds.is_constructed(); }
};

#endif /* _INCLUDE__OS__ATTACHED_ROM_DATASPACE_H_ */
