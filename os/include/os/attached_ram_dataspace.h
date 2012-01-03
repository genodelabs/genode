/*
 * \brief  RAM dataspace utility
 * \author Norman Feske
 * \date   2008-03-22
 *
 * The combination of RAM allocation and a local RM attachment
 * is a frequent use case. Each function may fail, which makes
 * error handling inevitable. This utility class encapsulates
 * this functionality to handle both operations as a transaction.
 * When embedded as a member, this class also takes care about
 * freeing and detaching the dataspace at destruction time.
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__ATTACHED_RAM_DATASPACE_H_
#define _INCLUDE__OS__ATTACHED_RAM_DATASPACE_H_

#include <ram_session/ram_session.h>
#include <base/env.h>

namespace Genode {

	class Attached_ram_dataspace
	{
		private:

			size_t                    _size;
			Ram_session              *_ram_session;
			Ram_dataspace_capability  _ds;
			void                     *_local_addr;

		public:

			/**
			 * Constructor
			 *
			 * \throw Ram_session::Alloc_failed
			 * \throw Rm_session::Attach_failed
			 */
			Attached_ram_dataspace(Ram_session *ram_session, size_t size)
			: _size(size), _ram_session(ram_session)
			{
				try {
					_ds = ram_session->alloc(size);
					_local_addr = env()->rm_session()->attach(_ds);

				/* revert allocation if attaching the dataspace failed */
				} catch (Rm_session::Attach_failed) {
					ram_session->free(_ds);
					throw;
				}
			}

			/**
			 * Destructor
			 */
			~Attached_ram_dataspace()
			{
				env()->rm_session()->detach(_local_addr);
				_ram_session->free(_ds);
			}

			/**
			 * Return capability of the used RAM dataspace
			 */
			Ram_dataspace_capability cap() const { return _ds; }

			/**
			 * Request local address
			 *
			 * This is a template to avoid inconvenient casts at
			 * the caller. A newly allocated RAM dataspace is
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

#endif /* _INCLUDE__OS__ATTACHED_RAM_DATASPACE_H_ */
