/*
 * \brief  Kernel object handling in core
 * \author Stefan Kalkowski
 * \date   2015-04-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__OBJECT_H_
#define _CORE__INCLUDE__OBJECT_H_

/* Genode includes */
#include <util/construct_at.h>

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
class Genode::Kernel_object
{
	private:

		uint8_t _data[sizeof(Kernel::Core_object<T>)]
			__attribute__((aligned(sizeof(addr_t))));

	protected:

		Untyped_capability _cap;

	public:

		Kernel_object() {}

		/**
		 * Creates a kernel object either via a syscall or directly
		 */
		template <typename... ARGS>
		Kernel_object(bool syscall, ARGS &&... args)
		: _cap(Capability_space::import(syscall ? T::syscall_create(&_data, args...)
		                                        : Kernel::cap_id_invalid()))
		{
			if (!syscall) construct_at<T>(&_data, args...);
		}

		~Kernel_object() { T::syscall_destroy(kernel_object()); }

		T * kernel_object() { return reinterpret_cast<T*>(_data); }

		/**
		 * Create the kernel object explicitely via this function
		 */
		template <typename... ARGS>
		bool create(ARGS &&... args)
		{
			if (_cap.valid()) return false;
			_cap = Capability_space::import(T::syscall_create(&_data, args...));
			return _cap.valid();
		}
};

#endif /* _CORE__INCLUDE__OBJECT_H_ */
