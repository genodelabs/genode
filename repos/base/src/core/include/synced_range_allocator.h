/*
 * \brief  Lock-guarded allocator interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2008-08-05
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SYNCED_RANGE_ALLOCATOR_H_
#define _CORE__INCLUDE__SYNCED_RANGE_ALLOCATOR_H_

#include <base/allocator.h>
#include <base/synced_interface.h>

namespace Genode {
	class Mapped_mem_allocator;
	template <typename> class Synced_range_allocator;
}


/**
 * Lock-guarded range allocator
 *
 * This class wraps the complete 'Range_allocator' interface while
 * preventing concurrent calls to the wrapped allocator implementation.
 *
 * \param ALLOC  class implementing the 'Range_allocator' interface
 */
template <typename ALLOC>
class Genode::Synced_range_allocator : public Range_allocator
{
	private:

		friend class Mapped_mem_allocator;

		Lock                          _default_lock { };
		Lock                         &_lock;
		ALLOC                         _alloc;
		Synced_interface<ALLOC, Lock> _synced_object;

	public:

		using Guard = typename Synced_interface<ALLOC, Lock>::Guard;

		template <typename... ARGS>
		Synced_range_allocator(Lock &lock, ARGS &&... args)
		: _lock(lock), _alloc(args...), _synced_object(_lock, &_alloc) { }

		template <typename... ARGS>
		Synced_range_allocator(ARGS &&... args)
		: _lock(_default_lock), _alloc(args...),
		_synced_object(_lock, &_alloc) { }

		Guard operator () ()       { return _synced_object(); }
		Guard operator () () const { return _synced_object(); }

		void print(Output &out) const { _synced_object()->print(out); }


		/*************************
		 ** Allocator interface **
		 *************************/

		bool alloc(size_t size, void **out_addr) override {
			return _synced_object()->alloc(size, out_addr); }

		void free(void *addr, size_t size) override {
			_synced_object()->free(addr, size); }

		size_t consumed() const override {
			return _synced_object()->consumed(); }

		size_t overhead(size_t size) const override {
			return _synced_object()->overhead(size); }

		bool need_size_for_free() const override {
			return _synced_object()->need_size_for_free(); }


		/*******************************
		 ** Range-allocator interface **
		 *******************************/

		int add_range(addr_t base, size_t size) override {
			return _synced_object()->add_range(base, size); }

		int remove_range(addr_t base, size_t size) override {
			return _synced_object()->remove_range(base, size); }

		Alloc_return alloc_aligned(size_t size, void **out_addr, int align,
		                           addr_t from = 0, addr_t to = ~0UL) override {
			return _synced_object()->alloc_aligned(size, out_addr, align, from, to); }

		Alloc_return alloc_addr(size_t size, addr_t addr) override {
			return _synced_object()->alloc_addr(size, addr); }

		void free(void *addr) override {
			_synced_object()->free(addr); }

		size_t avail() const override {
			return _synced_object()->avail(); }

		bool valid_addr(addr_t addr) const override {
			return _synced_object()->valid_addr(addr); }
};

#endif /* _CORE__INCLUDE__SYNCED_RANGE_ALLOCATOR_H_ */
