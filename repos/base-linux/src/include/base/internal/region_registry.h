/*
 * \brief  Registry of virtual-memory regions
 * \author Norman Feske
 * \date   2016-04-29
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__REGION_REGISTRY_
#define _INCLUDE__BASE__INTERNAL__REGION_REGISTRY_

#include <dataspace/capability.h>
#include <base/log.h>

namespace Genode {
	class Region;
	class Region_registry;
}


class Genode::Region
{
	private:

		addr_t               _start;
		off_t                _offset;
		Dataspace_capability _ds;
		size_t               _size;

		/**
		 * Return offset of first byte after the region
		 */
		addr_t _end() const { return _start + _size; }

	public:

		Region() : _start(0), _offset(0), _size(0) { }

		Region(addr_t start, off_t offset, Dataspace_capability ds, size_t size)
		: _start(start), _offset(offset), _ds(ds), _size(size) { }

		bool                 used()      const { return _size > 0; }
		addr_t               start()     const { return _start; }
		off_t                offset()    const { return _offset; }
		size_t               size()      const { return _size; }
		Dataspace_capability dataspace() const { return _ds; }

		bool intersects(Region const &r) const
		{
			return (r.start() < _end()) && (_start < r._end());
		}
};


class Genode::Region_registry
{
	public:

		enum { MAX_REGIONS = 4096 };

	private:

		Region _map[MAX_REGIONS];

		bool _id_valid(int id) const {
			return (id >= 0 && id < MAX_REGIONS); }

	public:

		/**
		 * Add region to region map
		 *
		 * \return region ID, or
		 *         -1 if out of metadata, or
		 *         -2 if region conflicts existing region
		 */
		int add_region(Region const &region)
		{
			/*
			 * Check for region conflicts
			 */
			for (int i = 0; i < MAX_REGIONS; i++) {
				if (_map[i].intersects(region))
					return -2;
			}

			/*
			 * Allocate new region metadata
			 */
			int i;
			for (i = 0; i < MAX_REGIONS; i++)
				if (!_map[i].used()) break;

			if (i == MAX_REGIONS) {
				error("maximum number of ", (unsigned)MAX_REGIONS, " regions reached");
				return -1;
			}

			_map[i] = region;
			return i;
		}

		Region region(int id) const
		{
			return _id_valid(id) ? _map[id] : Region();
		}

		Region lookup(addr_t start)
		{
			for (int i = 0; i < MAX_REGIONS; i++)
				if (_map[i].start() == start)
					return _map[i];
			return Region();
		}

		void remove_region(addr_t start)
		{
			for (int i = 0; i < MAX_REGIONS; i++)
				if (_map[i].start() == start)
					_map[i] = Region();
		}
};

#endif /* _INCLUDE__BASE__INTERNAL__REGION_REGISTRY_ */
