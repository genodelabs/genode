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

#ifndef _INCLUDE__BASE__SYNCED_ALLOCATOR_H_
#define _INCLUDE__BASE__SYNCED_ALLOCATOR_H_

#include <base/allocator.h>
#include <base/synced_interface.h>

namespace Genode {
	template <typename> class Synced_allocator;
}


/**
 * Lock-guarded allocator
 *
 * This class wraps the complete 'Allocator' interface while
 * preventing concurrent calls to the wrapped allocator implementation.
 *
 * \param ALLOC  class implementing the 'Allocator' interface
 */
template <typename ALLOC>
class Genode::Synced_allocator : public Allocator
{
	private:

		Lock                          _lock { };
		ALLOC                         _alloc;
		Synced_interface<ALLOC, Lock> _synced_object;

	public:

		using Guard = typename Synced_interface<ALLOC, Lock>::Guard;

		template <typename... ARGS>
		Synced_allocator(ARGS &&... args)
		: _alloc(args...), _synced_object(_lock, &_alloc) { }

		Guard operator () ()       { return _synced_object(); }
		Guard operator () () const { return _synced_object(); }


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
};

#endif /* _INCLUDE__BASE__SYNCED_ALLOCATOR_H_ */
