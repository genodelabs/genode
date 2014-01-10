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
			void                     *_local_addr = 0;

			void _detach()
			{
				if (!_local_addr)
					return;

				env()->rm_session()->detach(_local_addr);
				_local_addr = 0;
				_size       = 0;
			}

			void _attach()
			{
				if (_local_addr)
					_detach();

				_ds = _rom.dataspace();
				if (_ds.valid()) {
					_size       = Dataspace_client(_ds).size();
					_local_addr = env()->rm_session()->attach(_ds);
				}
			}

		public:

			/**
			 * Constructor
			 *
			 * \throw Rom_connection::Rom_connection_failed
			 * \throw Rm_session::Attach_failed
			 */
			Attached_rom_dataspace(char const *name)
			: _rom(name) { _attach(); }

			/**
			 * Destructor
			 */
			~Attached_rom_dataspace() { _detach(); }

			/**
			 * Return capability of the used ROM dataspace
			 */
			Rom_dataspace_capability cap() const { return _ds; }

			/**
			 * Request local address
			 *
			 * This is a template to avoid inconvenient casts at the caller.
			 * A newly allocated ROM dataspace is untyped memory anyway.
			 */
			template <typename T>
			T *local_addr() { return static_cast<T *>(_local_addr); }

			/**
			 * Return size
			 */
			size_t size() const { return _size; }

			/**
			 * Register signal handler for ROM module changes
			 */
			void sigh(Signal_context_capability sigh) { _rom.sigh(sigh); }

			/**
			 * Re-attach ROM module
			 */
			void update() { _attach(); }

			/**
			 * Return true of content is present
			 */
			bool is_valid() const { return _local_addr != 0; }
	};
}

#endif /* _INCLUDE__OS__ATTACHED_ROM_DATASPACE_H_ */
