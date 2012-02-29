/*
 * \brief  Interface for process-local capability-selector allocation
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-01-19
 *
 * This interface is Fiasco-specific and not part of the Genode API. It should
 * only be used internally by the framework or by Fiasco-specific code. The
 * implementation of the interface is part of the environment library.
 *
 * This implementation is borrowed by the nova-platform equivalent.
 * (TODO: merge it)
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CAP_SEL_ALLOC_H_
#define _INCLUDE__BASE__CAP_SEL_ALLOC_H_

/* Genode includes */
#include <base/stdint.h>
#include <util/avl_tree.h>
#include <base/native_types.h>
#include <base/printf.h>
#include <base/lock_guard.h>
#include <cpu/atomic.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/consts.h>
}

namespace Genode
{
	class Capability_allocator
	{
		protected:

			Capability_allocator() {}
			virtual ~Capability_allocator() {}

		public:

			/**
			 * Allocate range of capability selectors
			 *
			 * \param num_caps_log2  number of capability selectors. By default,
			 *                       the function returns a single capability selector.
			 * \return               first capability selector of allocated range,
			 *                       or 0 if allocation failed
			 */
			virtual addr_t alloc(size_t num_caps = 1) = 0;


			/**
			 * Allocate or find a capability selector
			 *
			 * \param id  Genode's global capability id we're looking for
			 * \return    return a previously allocated cap-selector associated
			 *            with the given id, or a new one, that is associated
			 *            with the id from now on.
			 */
			virtual addr_t alloc_id(unsigned id) = 0;

			/**
			 * Release range of capability selectors
			 *
			 * \param cap            first capability selector of range
			 * \param num_caps_log2  number of capability selectors to free.
			 */
			virtual void free(addr_t cap, size_t num_caps = 1) = 0;
	};


	Capability_allocator* cap_alloc();


	/**
	 * Low-level spin-lock to protect the allocator
	 *
	 * We cannot use a normal Genode lock because this lock is used by code
	 * executed prior the initialization of Genode.
	 */
	class Spin_lock
	{
		private:

			volatile int _spinlock;

		public:

			/**
			 * Constructor
			 */
			Spin_lock();

			void lock();
			void unlock();

			/**
			 * Lock guard
			 */
			typedef Genode::Lock_guard<Spin_lock> Guard;
	};



	template <unsigned SZ>
	class Capability_allocator_tpl : public Capability_allocator
	{
		private:

			/**
			 * Node in the capability cache,
			 * associates global cap ids with kernel-capabilities.
			 */
			class Cap_node : public Avl_node<Cap_node>
			{
				private:

					friend class Capability_allocator_tpl<SZ>;

					unsigned long _id;
					addr_t        _kcap;

				public:

					Cap_node() : _id(0), _kcap(0) {}
					Cap_node(unsigned long id, addr_t kcap)
					: _id(id), _kcap(kcap) {}

					bool higher(Cap_node *n) {
						return n->_id > _id; }

					Cap_node *find_by_id(unsigned long id)
					{
						if (_id == id) return this;

						Cap_node *n = Avl_node<Cap_node>::child(id > _id);
						return n ? n->find_by_id(id) : 0;
					}


					unsigned long id()    const { return _id;          }
					addr_t        kcap()  const { return _kcap;        }
					bool          valid() const { return _id || _kcap; }
			};


			addr_t             _cap_idx;  /* start cap-selector */
			Cap_node           _data[SZ]; /* cache-nodes backing store */
			Avl_tree<Cap_node> _tree;     /* cap cache */
			Spin_lock          _lock;

		public:

			/**
			 * Constructor
			 */
			Capability_allocator_tpl()
			: _cap_idx(Fiasco::Fiasco_capability::USER_BASE_CAP) { }


			/************************************
			 ** Capability_allocator interface **
			 ************************************/

			addr_t alloc(size_t num_caps)
			{
				Spin_lock::Guard guard(_lock);

				int ret_base = _cap_idx;
				_cap_idx += num_caps * Fiasco::L4_CAP_SIZE;
				return ret_base;
			}

			addr_t alloc_id(unsigned id)
			{
				_lock.lock();
				Cap_node *n = _tree.first();
				if (n)
					n = n->find_by_id(id);
				_lock.unlock();

				if (n) {
					return n->kcap();
				}

				addr_t kcap = alloc(1);

				_lock.lock();
				for (unsigned i = 0; i < SZ; i++)
					if (!_data[i].valid()) {
						_data[i]._id   = id;
						_data[i]._kcap = kcap;
						_tree.insert(&_data[i]);
						break;
					}
				_lock.unlock();
				return kcap;
			}

			void free(addr_t cap, size_t num_caps)
			{
				Spin_lock::Guard guard(_lock);

				for (unsigned i = 0; i < SZ; i++)
					if (!_data[i]._kcap == cap) {
						_tree.remove(&_data[i]);
						_data[i]._kcap = 0;
						_data[i]._id   = 0;
						break;
					}
			}
	};
}

#endif /* _INCLUDE__BASE__CAP_SEL_ALLOC_H_ */

