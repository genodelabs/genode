/*
 * \brief  Kernel object handling in core
 * \author Stefan Kalkowski
 * \date   2015-04-21
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__OBJECT_H_
#define _CORE__OBJECT_H_

/* Genode includes */
#include <util/reconstructible.h>

/* base-internal includes */
#include <base/internal/capability_space.h>

/* base-hw includes */
#include <kernel/interface.h>
#include <kernel/object.h>

namespace Genode {
	/**
	 * Represents a kernel object in core
	 *
	 * \param T  type of the kernel object
	 */
	template <typename T> class Kernel_object;
}


template <typename T>
class Genode::Kernel_object : public Genode::Constructible<Kernel::Core_object<T>>
{
	protected:

		Untyped_capability _cap { };

	public:

		Kernel_object() {}

		/**
		 * Creates a kernel object either via a syscall or directly
		 */
		template <typename... ARGS>
		Kernel_object(bool syscall, ARGS &&... args)
		: _cap(Capability_space::import(syscall ? T::syscall_create(*this, args...)
		                                        : Kernel::cap_id_invalid()))
		{
			if (!syscall) Genode::Constructible<Kernel::Core_object<T>>::construct(args...);
		}

		~Kernel_object()
		{
			if (Genode::Constructible<Kernel::Core_object<T>>::constructed())
				T::syscall_destroy(*this);
		}

		Untyped_capability cap() { return _cap; }

		/**
		 * Create the kernel object explicitely via this function
		 */
		template <typename... ARGS>
		bool create(ARGS &&... args)
		{
			if (Genode::Constructible<Kernel::Core_object<T>>::constructed())
				return false;

			_cap = Capability_space::import(T::syscall_create(*this, args...));
			return _cap.valid();
		}
};

#endif /* _CORE__OBJECT_H_ */
