/*
 * \brief  A guard for arbitrary allocators to limit memory exhaustion
 * \author Stefan Kalkowski
 * \date   2010-08-20
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ALLOCATOR_GUARD_H_
#define _ALLOCATOR_GUARD_H_

#include <base/allocator.h>
#include <base/printf.h>
#include <base/stdint.h>

namespace Genode {

	/**
	 * This class acts as guard for arbitrary allocators to limit
	 * memory exhaustion
	 */
	class Allocator_guard : public Allocator
	{
		private:

			Allocator *_allocator;  /* allocator to guard */
			size_t     _amount;     /* total amount */
			size_t     _consumed;   /* already consumed bytes */

		public:

			Allocator_guard(Allocator *allocator, size_t amount)
			: _allocator(allocator), _amount(amount), _consumed(0) { }

			/**
			 * Extend allocation limit
			 */
			void upgrade(size_t additional_amount) {
				_amount += additional_amount; }

			/**
			 * Consume bytes without actually allocating them
			 */
			bool withdraw(size_t size)
			{
				if ((_amount - _consumed) < size)
					return false;

				_consumed += size;
				return true;
			}

			/*************************
			 ** Allocator interface **
			 *************************/

			/**
			 * Allocate block
			 *
			 * \param size      block size to allocate
			 * \param out_addr  resulting pointer to the new block,
			 *                  undefined in the error case
			 * \return          true on success
			 */
			bool alloc(size_t size, void **out_addr)
			{
				if ((_amount - _consumed) < (size + _allocator->overhead(size))) {
					PWRN("Quota exceeded! amount=%zd, size=%zd, consumed=%zd",
					     _amount, (size + _allocator->overhead(size)), _consumed);
					return false;
				}
				bool b = _allocator->alloc(size, out_addr);
				if (b)
					_consumed += size + _allocator->overhead(size);
				return b;
			}

			/**
			 * Free block a previously allocated block
			 */
			void free(void *addr, size_t size)
			{
				_allocator->free(addr, size);
				_consumed -= size + _allocator->overhead(size);
			}

			/**
			 * Return amount of backing store consumed by the allocator
			 */
			size_t consumed() { return _consumed; }

			/**
			 * Return allocation limit
			 */
			size_t quota() const { return _amount; }

			/**
			 * Return meta-data overhead per block
			 */
			size_t overhead(size_t size) { return _allocator->overhead(size); }
	};
}

#endif /* _ALLOCATOR_GUARD_H_ */
