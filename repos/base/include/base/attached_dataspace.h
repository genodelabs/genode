/*
 * \brief  Utility to attach a dataspace to the local address space
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ATTACHED_DATASPACE_H_
#define _INCLUDE__BASE__ATTACHED_DATASPACE_H_

#include <dataspace/client.h>
#include <base/local.h>

namespace Genode { class Attached_dataspace; }


class Genode::Attached_dataspace : Noncopyable
{
	public:

		struct Invalid_dataspace : Exception { };
		struct Region_conflict   : Exception { };

	private:

		using Local_rm = Local::Constrained_region_map;

		Dataspace_capability const _ds;

		Local_rm::Result _attached;

		template <typename T>
		T *_ptr() const
		{
			return _attached.convert<T *>(
				[&] (Local_rm::Attachment const &a) { return (T *)a.ptr; },
				[&] (Local_rm::Error)               { return nullptr; });
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw Region_conflict
		 * \throw Out_of_caps
		 * \throw Out_of_ram
		 */
		Attached_dataspace(Local_rm &rm, Dataspace_capability ds)
		:
			_ds(ds),
			_attached(rm.attach(ds, {
				.size       = { }, .offset    = { },
				.use_at     = { }, .at        = { },
				.executable = { }, .writeable = true
			}))
		{
			_attached.with_error([&] (Local_rm::Error e) {
				switch (e) {
				case Local_rm::Error::OUT_OF_RAM:        throw Out_of_ram();
				case Local_rm::Error::OUT_OF_CAPS:       throw Out_of_caps();
				case Local_rm::Error::REGION_CONFLICT:   throw Region_conflict();
				case Local_rm::Error::INVALID_DATASPACE: throw Invalid_dataspace();
				}
			});
		}

		/**
		 * Return capability of the used dataspace
		 */
		Dataspace_capability cap() const { return _ds; }

		/**
		 * Request local address
		 *
		 * This is a template to avoid inconvenient casts at the caller.
		 * A newly attached dataspace is untyped memory anyway.
		 */
		template <typename T>
		T *local_addr() { return _ptr<T>(); }

		template <typename T>
		T const *local_addr() const { return _ptr<T const>(); }

		/**
		 * Return size
		 */
		size_t size() const
		{
			return _attached.convert<size_t>(
				[&] (Local_rm::Attachment const &a) { return a.num_bytes; },
				[&] (Local_rm::Error)               { return 0UL; });
		}

		/**
		 * Return byte range of locally mapped dataspace
		 */
		Byte_range_ptr bytes() const { return { _ptr<char>(), size() }; }

		/**
		 * Forget dataspace, thereby skipping the detachment on destruction
		 *
		 * This method can be called if the the dataspace is known to be
		 * physically destroyed, e.g., because the session where the dataspace
		 * originated from was closed. In this case, core will already have
		 * removed the memory mappings of the dataspace. So we have to omit the
		 * detach operation in '~Attached_dataspace'.
		 */
		void invalidate() { _attached = Local_rm::Error::INVALID_DATASPACE; }
};

#endif /* _INCLUDE__BASE__ATTACHED_DATASPACE_H_ */
