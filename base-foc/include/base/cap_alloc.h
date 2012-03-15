/*
 * \brief  Capability index allocator for Fiasco.OC.
 * \author Stefan Kalkowski
 * \date   2012-02-16
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CAP_ALLOC_H_
#define _INCLUDE__BASE__CAP_ALLOC_H_

#include <base/cap_map.h>
#include <base/native_types.h>

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

			Spin_lock _lock; /* used very early in initialization,
			                    where normal lock isn't feasible */

			enum {
				/* everything above START_IDX is managed by core */
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

			Cap_index* alloc(size_t cnt)
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
				return 0;
			}

			Cap_index* alloc(addr_t addr, size_t cnt)
			{
				Lock_guard<Spin_lock> guard(_lock);

				/*
				 * construct the Cap_index pointer from the given
				 * address in capability space
				 */
				T* obj = reinterpret_cast<T*>(kcap_to_idx(addr));
				T* ret = obj;

				/* check whether the consecutive entries are in range and unused */
				for (size_t i = 0; i < cnt; i++, obj++) {
					if (obj < &_indices[0] || obj >= &_indices[SZ])
						throw Index_out_of_bounds();
					if (obj->used())
						throw Region_conflict();
					new (obj) T();
				}
				return ret;
			}

			void free(Cap_index* idx, size_t cnt)
			{
				Lock_guard<Spin_lock> guard(_lock);

				T* obj = static_cast<T*>(idx);
				for (size_t i = 0; i < cnt; obj++, i++) {
					/* range check given pointer address */
					if (obj < &_indices[0] || obj >= &_indices[SZ])
						throw Index_out_of_bounds();
					delete obj;
				}
			}

			addr_t idx_to_kcap(Cap_index *idx) {
				return ((T*)idx - &_indices[0]) << Fiasco::L4_CAP_SHIFT;
			}

			Cap_index* kcap_to_idx(addr_t kcap) {
				return &_indices[kcap >> Fiasco::L4_CAP_SHIFT]; }
	};
}

#endif /* _INCLUDE__BASE__CAP_ALLOC_H_ */
