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
	using Monitored_rpc_object::Monitored_rpc_object;

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

		return _real.call<Rpc_attach>(ds, size, offset, use_local_addr, local_addr,
		                              executable, writeable);
	}

	void detach(Local_addr local_addr) override
	{
		_real.call<Rpc_detach>(local_addr);
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
