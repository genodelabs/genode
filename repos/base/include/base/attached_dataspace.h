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
#include <base/env.h>

namespace Genode { class Attached_dataspace; }


class Genode::Attached_dataspace : Noncopyable
{
	public:

		struct Invalid_dataspace : Exception { };
		struct Region_conflict   : Exception { };

	private:

		Dataspace_capability _ds;

		Region_map &_rm;

		Dataspace_capability _check(Dataspace_capability ds)
		{
			if (ds.valid())
				return ds;

			throw Invalid_dataspace();
		}

		Region_map::Attach_result _attached = _rm.attach(_ds, {
			.size       = { }, .offset    = { },
			.use_at     = { }, .at        = { },
			.executable = { }, .writeable = true });

		template <typename T>
		T *_ptr() const
		{
			return _attached.convert<T *>(
				[&] (Region_map::Range range)  { return (T *)range.start; },
				[&] (Region_map::Attach_error) { return nullptr; });
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw Region_conflict
		 * \throw Invalid_dataspace
		 * \throw Out_of_caps
		 * \throw Out_of_ram
		 */
		Attached_dataspace(Region_map &rm, Dataspace_capability ds)
		:
			_ds(_check(ds)), _rm(rm)
		{
			_attached.with_error([&] (Region_map::Attach_error e) {
				if (e == Region_map::Attach_error::OUT_OF_RAM)  throw Out_of_ram();
				if (e == Region_map::Attach_error::OUT_OF_CAPS) throw Out_of_caps();
				throw Region_conflict();
			});
		}

		/**
		 * Destructor
		 */
		~Attached_dataspace()
		{
			_attached.with_result(
				[&] (Region_map::Range range)  { _rm.detach(range.start); },
				[&] (Region_map::Attach_error) { });
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
				[&] (Region_map::Range range)  { return range.num_bytes; },
				[&] (Region_map::Attach_error) { return 0UL; });
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
		void invalidate() { _attached = Region_map::Attach_error::INVALID_DATASPACE; }
};

#endif /* _INCLUDE__BASE__ATTACHED_DATASPACE_H_ */
