/*
 * \brief  Monitored region map
 * \author Norman Feske
 * \date   2023-05-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITORED_REGION_MAP_H_
#define _MONITORED_REGION_MAP_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <base/attached_ram_dataspace.h>
#include <region_map/region_map.h>

/* local includes */
#include <types.h>

namespace Monitor { struct Monitored_region_map; }


struct Monitor::Monitored_region_map : Monitored_rpc_object<Region_map>
{
	/* see the comment in base/include/region_map/client.h */
	Dataspace_capability _rm_ds_cap { };

	struct Writeable_text_segments
	{
		Allocator     &_alloc;
		Ram_allocator &_ram;
		Region_map    &_local_rm;

		struct Ram_ds : Registry<Ram_ds>::Element
		{
			Attached_ram_dataspace ds;

			Ram_ds(Registry<Ram_ds> &registry, Ram_allocator &ram, Region_map &local_rm,
			       Const_byte_range_ptr const &content)
			:
				Registry<Ram_ds>::Element(registry, *this),
				ds(ram, local_rm, content.num_bytes)
			{
				memcpy(ds.local_addr<void>(), content.start, content.num_bytes);
			}
		};

		Registry<Ram_ds> _dataspaces { };

		Writeable_text_segments(Allocator     &alloc,
		                        Ram_allocator &ram,
		                        Region_map    &local_rm)
		:
			_alloc(alloc), _ram(ram), _local_rm(local_rm)
		{ }

		~Writeable_text_segments()
		{
			_dataspaces.for_each([&] (Ram_ds &ram_ds) {
				destroy(_alloc, &ram_ds); });
		}

		Dataspace_capability create_writable_copy(Dataspace_capability orig_ds,
		                                          off_t offset, size_t size)
		{
			Attached_dataspace ds { _local_rm, orig_ds };

			if (size_t(offset) >= ds.size())
				return Dataspace_capability();

			Const_byte_range_ptr const
				content_ptr(ds.local_addr<char>() + offset,
				            min(size, ds.size() - size_t(offset)));

			Ram_ds &ram_ds = *new (_alloc)
				Ram_ds(_dataspaces, _ram, _local_rm, content_ptr);

			return ram_ds.ds.cap();
		}
	};

	Constructible<Writeable_text_segments> _writeable_text_segments { };


	void writeable_text_segments(Allocator    &alloc,
	                            Ram_allocator &ram,
	                             Region_map   &local_rm)
	{
		if (!_writeable_text_segments.constructed())
			_writeable_text_segments.construct(alloc, ram, local_rm);
	}

	struct Region : List<Region>::Element
	{
		Dataspace_capability cap;
		addr_t               addr;
		size_t               size;
		bool                 writeable;

		Region(Dataspace_capability cap, addr_t addr, size_t size,
		       bool writeable)
		: cap(cap), addr(addr), size(size), writeable(writeable) { }
	};

	List<Region> _regions { };

	void for_each_region(auto const &fn) const
	{
		for (Region const *region = _regions.first(); region; region = region->next())
			fn(*region);
	}

	Allocator &_alloc;

	Monitored_region_map(Entrypoint &ep, Capability<Region_map> real,
	                     Name const &name, Allocator &alloc)
	: Monitored_rpc_object(ep, real, name),
	  _alloc(alloc) { }

	~Monitored_region_map()
	{
		while (Region *region = _regions.first()) {
			_regions.remove(region);
			destroy(_alloc, region);
		}
	}

	/**************************
	 ** Region_map interface **
	 **************************/

	Local_addr attach(Dataspace_capability ds, size_t size = 0,
	                  off_t offset = 0, bool use_local_addr = false,
	                  Local_addr local_addr = (void *)0,
	                  bool executable = false,
	                  bool writeable = true) override
	{
		if (executable && !writeable && _writeable_text_segments.constructed()) {
			ds = _writeable_text_segments->create_writable_copy(ds, offset, size);
			offset = 0;
			writeable = true;
		}

		Local_addr attached_addr = _real.call<Rpc_attach>(ds, size, offset,
		                                                  use_local_addr,
		                                                  local_addr,
		                                                  executable,
		                                                  writeable);
		size_t region_size = size ? size :
		                     (Dataspace_client(ds).size() - offset);
		enum { PAGE_SIZE_LOG2 = 12 };
		region_size = align_addr(region_size, PAGE_SIZE_LOG2);

		/*
		 * It can happen that previous attachments got implicitly
		 * removed by destruction of the dataspace without knowledge
		 * of the monitor. The newly obtained region could then
		 * overlap with outdated region list entries which must
		 * be removed before inserting the new region.
		 */

		addr_t start_addr = (addr_t)attached_addr;
		addr_t end_addr = start_addr + region_size - 1;

		for (Region *region = _regions.first(); region; ) {

			if ((region->addr <= end_addr) &&
			    ((region->addr + region->size - 1) >= start_addr)) {

				Region *next_region = region->next();
				_regions.remove(region);
				destroy(_alloc, region);
				region = next_region;
				continue;
			}

			region = region->next();
		}

		_regions.insert(new (_alloc) Region(ds, (addr_t)attached_addr,
		                                    region_size, writeable));
		return attached_addr;
	}

	void detach(Local_addr local_addr) override
	{
		_real.call<Rpc_detach>(local_addr);

		addr_t addr = (addr_t)local_addr;

		for (Region *region = _regions.first(); region; region = region->next()) {
			if ((addr >= region->addr) &&
			    (addr <= (region->addr + region->size - 1))) {
				_regions.remove(region);
				destroy(_alloc, region);
				break;
			}
		}
	}

	void fault_handler(Signal_context_capability) override
	{
		warning("Monitored_region_map: ignoring custom fault_handler for ", _name);
	}

	State state() override
	{
		return _real.call<Rpc_state>();
	}

	Dataspace_capability dataspace() override
	{
		if (!_rm_ds_cap.valid())
			_rm_ds_cap = _real.call<Rpc_dataspace>();

		return _rm_ds_cap;
	}
};

#endif /* _MONITORED_REGION_MAP_H_ */
