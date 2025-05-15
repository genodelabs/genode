/*
 * \brief  Error types and last-resort error handling
 * \author Norman Feske
 * \date   2025-03-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ERROR_H_
#define _INCLUDE__BASE__ERROR_H_

namespace Genode {

	/**
	 * Common error returned by constrained allocators
	 *
	 * OUT_OF_RAM and OUT_OF_CAPS can in principle be resolved by upgrading
	 * the resource budget of the allocator.
	 *
	 * DENIED expresses a situation where the allocator cannot satisfy the
	 * allocation for unresolvable reasons. For example, the allocator may
	 * have a hard limit of the number of allocations, or the allocation of
	 * a large contiguous range is prevented by internal fragmentation, or
	 * a requested alignment constraint cannot be met. In these cases, the
	 * allocator reflects the condition to the caller to stay healthy and let
	 * the caller fail gracefully or consciously panic at the caller side.
	 */
	enum class Alloc_error { OUT_OF_RAM, OUT_OF_CAPS, DENIED };

	/**
	 * Common error returned when exhaustion a destination buffer
	 */
	enum class Buffer_error { EXCEEDED };

	/**
	 * Error conditions during session-creation
	 */
	enum class Session_error {
		DENIED,             /* parent or server denies request */
		OUT_OF_RAM,         /* session RAM quota exceeds our resources */
		OUT_OF_CAPS,        /* session CAP quota exceeds our resources */
		INSUFFICIENT_RAM,   /* RAM donation does not suffice */
		INSUFFICIENT_CAPS,  /* CAP donation does not suffice */
	};

	/**
	 * Raise an error without return
	 *
	 * This function should never be called except in panic situations where
	 * no other way of reflecting an error condition exists.
	 *
	 * When using the C++ runtime, errors are reflected by the exceptions
	 * defined at 'base/exception.h'. If the component has no reference to
	 * 'raise()' C++ exceptions are disabled, the component is known to
	 * contain no unhandled error conditions.
	 */
	void raise(Alloc_error) __attribute__((noreturn));

	/**
	 * Error conditions of panic situations
	 *
	 * Those conditions should never occur in well-behaving programs.
	 */
	enum class Unexpected_error {
		INDEX_OUT_OF_BOUNDS,       /* 'Array' accessed w/o index validation */
		NONEXISTENT_SUB_NODE,      /* use of 'sub_node' instead of 'with_sub_node' */
		ACCESS_UNCONSTRUCTED_OBJ,  /* missing check of 'constructed()' */
		IPC_BUFFER_EXCEEDED,       /* IPC marshalling/unmarshalling */
	};

	/**
	 * Raise an unexpected error
	 *
	 * As the occurrence of an 'Unexpected_error' conditions hints at
	 * programming bug, the function prints a backtrace and reflects the
	 * situation by throwing the matching exception defined at
	 * _base/exception.h_.
	 */
	void raise(Unexpected_error) __attribute__((noreturn));
}

#endif /* _INCLUDE__BASE__ERROR_H_ */
