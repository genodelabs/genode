/*
 * \brief  Capability index allocator for Fiasco.OC.
 * \author Stefan Kalkowski
 * \date   2012-02-16
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__CAP_ALLOC_H_
#define _INCLUDE__BASE__CAP_ALLOC_H_

/* Genode includes */
#include <base/native_capability.h>
#include <util/construct_at.h>
#include <foc/native_capability.h>

/* base-internal includes */
#include <base/internal/cap_map.h>
#include <base/internal/foc_assert.h>

namespace Genode {

	/**
	 * Cap_index_allocator_tpl implements the Cap_index_allocator for Fiasco.OC.
	 *
	 * It's designed as a template because we need two distinguished versions
	 * for core and non-core processes with respect to dimensioning. Moreover,
	 * core needs more information within a Cap_index object, than the base
	 * class provides.
	 *
	 * \param T   Cap_index specialization to use
	 * \param SZ  size of Cap_index array used by the allocator
	 */
	template <typename T, unsigned SZ>
	class Cap_index_allocator_tpl : public Cap_index_allocator
	{
		private:

			/*
			 * Noncopyable
			 */
			Cap_index_allocator_tpl(Cap_index_allocator_tpl const &);
			Cap_index_allocator_tpl &operator = (Cap_index_allocator_tpl const &);

			Spin_lock _lock { }; /* used very early in initialization,
			                        where normal lock isn't feasible */

			enum {
				/* everything below START_IDX is managed by core */
				START_IDX = Fiasco::USER_BASE_CAP >> Fiasco::L4_CAP_SHIFT
			};

		protected:

			unsigned char _data[SZ*sizeof(T)];
			T*            _indices;

		public:

			Cap_index_allocator_tpl() : _indices(reinterpret_cast<T*>(&_data)) {
				memset(&_data, 0, sizeof(_data)); }


			/***********************************
			 ** Cap_index_allocator interface **
			 ***********************************/

			Cap_index* alloc_range(size_t cnt)
			{
				Lock_guard<Spin_lock> guard(_lock);

				/*
				 * iterate through array and find unused, consecutive entries
				 */
				for (unsigned i = START_IDX, j = 0; (i+cnt) < SZ; i+=j+1, j=0) {
					for (; j < cnt; j++)
						if (_indices[i+j].used())
							break;

					/* if we found a fitting hole, initialize the objects */
					if (j == cnt) {
						for (j = 0; j < cnt; j++)
							new (&_indices[i+j]) T();
						return &_indices[i];
					}
				}
				ASSERT(0, "cap index allocation failed");
				return 0;
			}

			Cap_index* alloc(addr_t addr)
			{
				Lock_guard<Spin_lock> guard(_lock);

				/*
				 * construct the Cap_index pointer from the given
				 * address in capability space
				 */
				T* obj = reinterpret_cast<T*>(kcap_to_idx(addr));

				if (obj < &_indices[0] || obj >= &_indices[SZ]) {
					ASSERT(0, "cap index out of bounds");
					throw Index_out_of_bounds();
				}

				return new (obj) T();
			}

			void free(Cap_index* idx, size_t cnt)
			{
				Lock_guard<Spin_lock> guard(_lock);

				T* obj = static_cast<T*>(idx);
				for (size_t i = 0; i < cnt; obj++, i++) {
					/* range check given pointer address */
					if (obj < &_indices[0] || obj >= &_indices[SZ]) {
						ASSERT(0, "cap index out of bounds");
						throw Index_out_of_bounds();
					}
					delete obj;
				}
			}

			addr_t idx_to_kcap(Cap_index const *idx) const {
				return ((T const *)idx - &_indices[0]) << Fiasco::L4_CAP_SHIFT;
			}

			Cap_index* kcap_to_idx(addr_t kcap) {
				return &_indices[kcap >> Fiasco::L4_CAP_SHIFT]; }

			bool static_idx(Cap_index *idx) {
				return ((T*)idx) < &_indices[START_IDX]; }

			void reinit()
			{
				construct_at<Cap_index_allocator_tpl<T, SZ> >(this);
			}

			size_t max_caps() const override { return SZ; }
	};
}

#endif /* _INCLUDE__BASE__CAP_ALLOC_H_ */
