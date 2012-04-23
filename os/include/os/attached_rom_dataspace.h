/*
 * \brief  ROM dataspace utility
 * \author Norman Feske
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__ATTACHED_ROM_DATASPACE_H_
#define _INCLUDE__OS__ATTACHED_ROM_DATASPACE_H_

#include <rom_session/connection.h>
#include <dataspace/client.h>
#include <base/env.h>

namespace Genode {

	class Attached_rom_dataspace
	{
		private:

			Rom_connection            _rom;
			Rom_dataspace_capability  _ds;
			size_t                    _size;
			void                     *_local_addr;

		public:

			/**
			 * Constructor
			 *
			 * \throw Rom_connection::Rom_connection_failed
			 * \throw Rm_session::Attach_failed
			 */
			Attached_rom_dataspace(char const *name)
			:
				_rom(name),
				_ds(_rom.dataspace()),
				_size(Dataspace_client(_ds).size()),
				_local_addr(env()->rm_session()->attach(_ds))
			{ }

			/**
			 * Destructor
			 */
			~Attached_rom_dataspace()
			{
				env()->rm_session()->detach(_local_addr);
			}

			/**
			 * Return capability of the used ROM dataspace
			 */
			Rom_dataspace_capability cap() const { return _ds; }

			/**
			 * Request local address
			 *
			 * This is a template to avoid inconvenient casts at
			 * the caller. A newly allocated ROM dataspace is
			 * untyped memory anyway.
			 */
			template <typename T>
			T *local_addr() { return static_cast<T *>(_local_addr); }

			/**
			 * Return size
			 */
			size_t size() const { return _size; }
	};
}

#endif /* _INCLUDE__OS__ATTACHED_ROM_DATASPACE_H_ */
