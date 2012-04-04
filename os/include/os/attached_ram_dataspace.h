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

			template <typename T>
			static void _swap(T &v1, T &v2) { T tmp = v1; v1 = v2; v2 = tmp; }

			void _detach_and_free_dataspace()
			{
				if (_local_addr)
					env()->rm_session()->detach(_local_addr);

				if (_ram_session && _ds.valid())
					_ram_session->free(_ds);
			}

			void _alloc_and_attach()
			{
				if (!_size || !_ram_session) return;

				try {
					_ds         = _ram_session->alloc(_size);
					_local_addr = env()->rm_session()->attach(_ds);

				/* revert allocation if attaching the dataspace failed */
				} catch (Rm_session::Attach_failed) {
					_ram_session->free(_ds);
					throw;
				}
			}

		public:

			/**
			 * Constructor
			 *
			 * \throw Ram_session::Alloc_failed
			 * \throw Rm_session::Attach_failed
			 */
			Attached_ram_dataspace(Ram_session *ram_session, size_t size)
			: _size(size), _ram_session(ram_session), _local_addr(0)
			{
				_alloc_and_attach();
			}

			/**
			 * Destructor
			 */
			~Attached_ram_dataspace() { _detach_and_free_dataspace(); }

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

			void swap(Attached_ram_dataspace &other)
			{
				_swap(_size,        other._size);
				_swap(_ram_session, other._ram_session);
				_swap(_ds,          other._ds);
				_swap(_local_addr,  other._local_addr);
			}

			/**
			 * Re-allocate dataspace with a new size
			 *
			 * The content of the original dataspace is not retained.
			 */
			void realloc(Ram_session *ram_session, size_t new_size)
			{
				if (new_size < _size) return;

				_detach_and_free_dataspace();

				_size        = new_size;
				_ram_session = ram_session;

				_alloc_and_attach();
			}
	};
}

#endif /* _INCLUDE__OS__ATTACHED_RAM_DATASPACE_H_ */
