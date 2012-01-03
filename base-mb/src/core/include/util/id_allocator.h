/*
 * \brief  Allocator for ID-labeled resources
 * \author Martin Stein
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__ID_ALLOCATOR_H_
#define _INCLUDE__UTIL__ID_ALLOCATOR_H_

/**
 * \param  HOLDER_T    type that should hold ID's
 * \param  ID_T        type of the allocatable ID's should be
 *                     an enumeration type that expresses a variety
 *                     less than the CPUs word width to the power of 2
 * \param  BYTE_WIDTH  the CPU's bytewidth
 */
template<typename HOLDER_T, typename ID_T, unsigned BYTE_WIDTH>
class Id_allocator
{
	enum {
		ID_WIDTH = sizeof(ID_T)*BYTE_WIDTH,
		ID_RANGE = 1 << ID_WIDTH
	};

	ID_T _first_allocatable;
	ID_T _last_allocatable;

	bool      _id_in_use[ID_RANGE];
	HOLDER_T *_holder_by_id[ID_RANGE];

	public:

		Id_allocator() :
			_first_allocatable(0),
			_last_allocatable(ID_RANGE-1)
		{
			for (unsigned i = _first_allocatable;
			     i <= _last_allocatable; i++)

				_id_in_use[i]=false;
		}

		Id_allocator(ID_T first, ID_T last) :
			_first_allocatable(first),
			_last_allocatable(last)
		{
			for (unsigned i = _first_allocatable;
			     i <= _last_allocatable; i++)

				_id_in_use[i]=false;
		}

		ID_T allocate()
		{
			for (unsigned i = _first_allocatable;
			     i <= _last_allocatable; i++) {

				if (_id_in_use[i])
					continue;

				_id_in_use[i]   = true;
				_holder_by_id[i] = 0;
				return (ID_T)i;
			}

			PERR("All ID's in use");
			return (ID_T)0;
		}

		ID_T allocate(HOLDER_T* o)
		{
			for (unsigned i = _first_allocatable;
			     i <= _last_allocatable; i++) {

				if (_id_in_use[i])
					continue;

				_id_in_use[i]   = true;
				_holder_by_id[i] = o;
				return (ID_T)i;
			}

			PERR("All ID's in use");
			return (ID_T)0;
		}

		bool allocate(HOLDER_T *o, ID_T id)
		{
			if (id < _first_allocatable || id > _last_allocatable) {
				PERR("ID unallocatable");
				return false;
			}
			if (!_id_in_use[id]) {
				_id_in_use[id]   = true;
				_holder_by_id[id] = o;
				return true;
			}
			else{
				PERR("ID in use");
				return false;
			}
		}

		HOLDER_T *holder(ID_T id) { return _holder_by_id[id]; }

		void free(ID_T id)
		{
			_id_in_use[id]=false;
			_holder_by_id[id]=0;
		}
};

#endif /*_INCLUDE__UTIL__ID_ALLOCATOR_H_*/


