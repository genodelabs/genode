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
		                                          addr_t offset, size_t size)
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

	static bool _intersects(Range const &a, Range const &b)
	{
		addr_t const a_end = a.start + a.num_bytes - 1;
		addr_t const b_end = b.start + b.num_bytes - 1;
		return (b.start <= a_end) && (b_end >= a.start);
	}

	void writeable_text_segments(Allocator     &alloc,
	                             Ram_allocator &ram,
	                             Region_map    &local_rm)
	{
		if (!_writeable_text_segments.constructed())
			_writeable_text_segments.construct(alloc, ram, local_rm);
	}

	struct Region : Registry<Region>::Element
	{
		Dataspace_capability cap;
		Range                range;
		bool                 writeable;

		Region(Registry<Region> &registry, Dataspace_capability cap,
		       Range range, bool writeable)
		:
			Registry<Region>::Element(registry, *this),
			cap(cap), range(range), writeable(writeable)
		{ }
	};

	Registry<Region> _regions { };

	void for_each_region(auto const &fn) const
	{
		_regions.for_each([&] (Region const &region) {
			fn(region); });
	}

	Allocator &_alloc;

	Monitored_region_map(Entrypoint &ep, Capability<Region_map> real,
	                     Name const &name, Allocator &alloc)
	: Monitored_rpc_object(ep, real, name),
	  _alloc(alloc) { }

	~Monitored_region_map()
	{
		_regions.for_each([&] (Region &region) {
			destroy(_alloc, &region);
		});
	}

	/**************************
	 ** Region_map interface **
	 **************************/

	Attach_result attach(Dataspace_capability ds, Attr const &orig_attr) override
	{
		Attr attr = orig_attr;
		if (attr.executable && !attr.writeable && _writeable_text_segments.constructed()) {
			ds = _writeable_text_segments->create_writable_copy(ds, attr.offset, attr.size);
			attr.offset = 0;
			attr.writeable = true;
		}

		return _real.call<Rpc_attach>(ds, attr).convert<Attach_result>(
			[&] (Range const range) -> Attach_result {
				/*
				 * It can happen that previous attachments got implicitly
				 * removed by destruction of the dataspace without knowledge
				 * of the monitor. The newly obtained region could then
				 * overlap with outdated region registry entries which must
				 * be removed before inserting the new region.
				 */
				_regions.for_each([&] (Region &region) {
					if (_intersects(region.range, range))
						destroy(_alloc, &region); });

				try {
					new (_alloc) Region(_regions, ds, range, attr.writeable);
				}
				catch (Out_of_ram)  {
					_real.call<Rpc_detach>(range.start);
					return Attach_error::OUT_OF_RAM;
				}
				catch (Out_of_caps) {
					_real.call<Rpc_detach>(range.start);
					return Attach_error::OUT_OF_CAPS;
				}
				return range;
			},
			[&] (Attach_error e) { return e; }
		);
	}

	void detach(addr_t const at) override
	{
		_real.call<Rpc_detach>(at);

		_regions.for_each([&] (Region &region) {
			if (_intersects(region.range, Range { at, 1 }))
				destroy(_alloc, &region); });
	}

	void fault_handler(Signal_context_capability) override
	{
		warning("Monitored_region_map: ignoring custom fault_handler for ", _name);
	}

	Fault fault() override { return _real.call<Rpc_fault>(); }

	Dataspace_capability dataspace() override
	{
		if (!_rm_ds_cap.valid())
			_rm_ds_cap = _real.call<Rpc_dataspace>();

		return _rm_ds_cap;
	}
};

#endif /* _MONITORED_REGION_MAP_H_ */
