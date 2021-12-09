/*
 * \brief  Internal capability representation
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-06-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CAPABILITY_DATA_H_
#define _INCLUDE__BASE__INTERNAL__CAPABILITY_DATA_H_

/* Genode includes */
#include <base/capability.h>

/**
 * A Native_capability::Data object represents a single mapping of the global
 * capability id to the address in the local capability space.
 *
 * The address of the data object determines the location in the
 * (platform-specific) capability space of the component. Therefore it
 * shouldn't be copied around, but only referenced by e.g. Native_capability.
 */
class Genode::Native_capability::Data : public Avl_node<Data>
{
	public:

		using id_t = uint16_t;

	private:

		constexpr static uint16_t INVALID_ID = ~0;
		constexpr static uint16_t UNUSED     =  0;

		uint8_t _ref_cnt; /* reference counter    */
		id_t    _id;      /* global capability id */

	public:

		Data() : _ref_cnt(0), _id(INVALID_ID) { }

		bool     valid() const   { return _id != INVALID_ID; }
		bool     used()  const   { return _id != UNUSED;     }
		uint16_t id()    const   { return _id;               }
		void     id(uint16_t id) { _id = id;                 }
		uint8_t  inc();
		uint8_t  dec();
		addr_t   kcap() const;

		void  operator delete (void* idx) { memset(idx, 0, sizeof(Data)); }


		/************************
		 ** Avl node interface **
		 ************************/

		bool higher(Data *n);
		Data *find_by_id(id_t id);
};

#endif /* _INCLUDE__BASE__INTERNAL__CAPABILITY_DATA_H_ */

