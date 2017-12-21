/*
 * \brief  Utility to open a ROM session and locally attach its content
 * \author Norman Feske
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ATTACHED_ROM_DATASPACE_H_
#define _INCLUDE__BASE__ATTACHED_ROM_DATASPACE_H_

#include <util/reconstructible.h>
#include <util/xml_node.h>
#include <base/attached_dataspace.h>
#include <rom_session/connection.h>

namespace Genode { class Attached_rom_dataspace; }


class Genode::Attached_rom_dataspace
{
	private:

		Region_map    &_rm;
		Rom_connection _rom;

		/*
		 * A ROM module may change or disappear over the lifetime of a ROM
		 * session. In contrast to the plain 'Attached_dataspace', which is
		 * always be valid once constructed, a 'Attached_rom_dataspace' has
		 * to handle the validity of the dataspace.
		 */
		Constructible<Attached_dataspace> _ds { };

		/**
		 * Try to attach the ROM module, ignore invalid dataspaces
		 */
		void _try_attach()
		{
			/*
			 * Normally, '_ds.construct()' would implicitly destruct an
			 * existing dataspace upon re-construction. However, we have to
			 * explicitly destruct the original dataspace prior calling
			 * '_rom.dataspace()'.
			 *
			 * The ROM server may destroy the original dataspace when the
			 * 'dataspace()' method is called. In this case, all existing
			 * mappings of the dataspace will be flushed by core. A destruction
			 * of 'Attached_dataspace' after this point will attempt to detach
			 * the already flushed mappings, thereby producing error messages
			 * at core.
			 */
			_ds.destruct();
			try { _ds.construct(_rm, _rom.dataspace()); }
			catch (Attached_dataspace::Invalid_dataspace) { }
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw Rom_connection::Rom_connection_failed
		 * \throw Region_map::Region_conflict
		 * \throw Region_map::Invalid_dataspace
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 */
		Attached_rom_dataspace(Env &env, char const *name)
		: _rm(env.rm()), _rom(env, name) { _try_attach(); }

		/**
		 * Constructor
		 *
		 * \noapi
		 * \deprecated  Use the constructor with 'Env &' as first
		 *              argument instead
		 */
		Attached_rom_dataspace(char const *name) __attribute__((deprecated))
		: _rm(*env_deprecated()->rm_session()), _rom(false /* deprecated */, name)
		{ _try_attach(); }

		/**
		 * Return capability of the used dataspace
		 */
		Dataspace_capability cap() const { return _ds->cap(); }

		template <typename T> T *local_addr() {
			return _ds->local_addr<T>(); }

		template <typename T> T const *local_addr() const {
			return _ds->local_addr<T const>(); }

		size_t size() const { return _ds->size(); }

		/**
		 * Register signal handler for ROM module changes
		 */
		void sigh(Signal_context_capability sigh) { _rom.sigh(sigh); }

		/**
		 * Update ROM module content, re-attach if needed
		 */
		void update()
		{
			/*
			 * If the dataspace is already attached and the update fits into
			 * the existing dataspace, we can keep everything in place. The
			 * dataspace content gets updated by the call of '_rom.update'.
			 */
			if (_ds.constructed() && _rom.update() == true)
				return;

			/*
			 * If there was no valid dataspace attached beforehand or the
			 * new data size exceeds the capacity of the existing dataspace,
			 * replace the current dataspace by a new one.
			 */
			_try_attach();
		}

		/**
		 * Return true of content is present
		 */
		bool valid() const { return _ds.constructed(); }

		/**
		 * Return true of content is present
		 *
		 * \noapi
		 * \deprecated use 'valid' instead
		 */
		bool is_valid() const { return valid(); }

		/**
		 * Return dataspace content as XML node
		 *
		 * This method always returns a valid XML node. It never throws an
		 * exception. If the dataspace is invalid or does not contain properly
		 * formatted XML, the returned XML node has the form "<empty/>".
		 */
		Xml_node xml() const
		{
			try {
				if (valid() && local_addr<void const>())
					return Xml_node(local_addr<char>(), size());
			} catch (Xml_node::Invalid_syntax) { }

			return Xml_node("<empty/>");
		}
};

#endif /* _INCLUDE__BASE__ATTACHED_ROM_DATASPACE_H_ */
