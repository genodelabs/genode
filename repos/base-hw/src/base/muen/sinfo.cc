/*
 * \brief  Muen subject information API impl
 * \author Reto Buerki
 * \date   2015-04-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <util/string.h>

#include <muen/sinfo.h>

#include "musinfo.h"

/* Log channel information */
static bool log_channel(
		const struct Genode::Sinfo::Channel_info * const channel,
		void *data)
{
	if (channel->has_event || channel->has_vector) {
		Genode::log("muen-sinfo: [",
		            channel->writable ? "writer" : "reader", " with ",
		            channel->has_event ? "event " : "vector", " ",
		            channel->has_event ? channel->event_number : channel->vector,
		            "] ", channel->name);
	} else {
		Genode::log("muen-sinfo: [",
		            channel->writable ? "writer" : "reader", " with no ",
		            channel->writable ? "event " : "vector", " ",
		            "] ", channel->name);
	}

	return true;
}


/* Log memory region information */
static bool log_memregion(const struct Genode::Sinfo::Memregion_info * const region,
                          void *data)
{
	Genode::log("muen-sinfo: [addr ", Genode::Hex(region->address), " "
	            "size ", Genode::Hex(region->size), " ",
	            region->writable ? "rw" : "ro",
	            region->executable ? "x" : "-",
	            "] ", region->name);
	return true;
}


/* Returns true if the given resource is a memory region */
static bool is_memregion(const struct resource_type * const resource)
{
	return resource->memregion_idx != NO_RESOURCE;
}


/* Returns true if the given resource is a channel */
static bool is_channel(const struct resource_type * const resource)
{
	return is_memregion(resource) && resource->channel_info_idx != NO_RESOURCE;
}


Sinfo::Sinfo(const addr_t base_addr)
{
	sinfo = ((subject_info_type *)base_addr);

	if (!check_magic()) {
		Genode::error("muen-sinfo: Subject information MAGIC mismatch");
		return;
	}
}


bool Sinfo::check_magic(void)
{
	return sinfo != 0 && sinfo->magic == MUEN_SUBJECT_INFO_MAGIC;
}


bool Sinfo::get_channel_info(const char * const name,
                             struct Channel_info *channel)
{
	int i;

	if (!check_magic())
		return false;

	for (i = 0; i < sinfo->resource_count; i++) {
		if (is_channel(&sinfo->resources[i]) &&
			strcmp(sinfo->resources[i].name.data, name) == 0) {
			fill_channel_data(i, channel);
			return true;
		}
	}
	return false;
}


bool Sinfo::get_memregion_info(const char * const name,
                               struct Memregion_info *memregion)
{
	int i;

	if (!check_magic())
		return false;

	for (i = 0; i < sinfo->resource_count; i++) {
		if (is_memregion(&sinfo->resources[i]) &&
			strcmp(sinfo->resources[i].name.data, name) == 0) {
			fill_memregion_data(i, memregion);
			return true;
		}
	}
	return false;
}


bool Sinfo::get_dev_info(const uint16_t sid, struct Dev_info *dev)
{
	int i;

	if (!check_magic())
		return false;

	for (i = 0; i < sinfo->dev_info_count; i++) {
		if (sinfo->dev_info[i].sid == sid) {
			fill_dev_data(i, dev);
			return true;
		}
	}
	return false;
}


bool Sinfo::for_each_channel(Channel_cb func, void *data)
{
	int i;
	struct Channel_info current_channel;

	if (!check_magic())
		return false;

	for (i = 0; i < sinfo->resource_count; i++) {
		if (is_channel(&sinfo->resources[i])) {
			fill_channel_data(i, &current_channel);
			if (!func(&current_channel, data))
				return false;
		}
	}
	return true;
}


bool Sinfo::for_each_memregion(Memregion_cb func, void *data)
{
	int i;
	struct Memregion_info current_region;

	if (!check_magic())
		return false;

	for (i = 0; i < sinfo->resource_count; i++) {
		if (is_memregion(&sinfo->resources[i])) {
			fill_memregion_data(i, &current_region);
			if (!func(&current_region, data))
				return false;
		}
	}
	return true;
}


uint64_t Sinfo::get_tsc_khz(void)
{
	if (!check_magic())
		return 0;

	return sinfo->tsc_khz;
}


uint64_t Sinfo::get_sched_start(void)
{
	if (!check_magic())
		return 0;

	return sinfo->tsc_schedule_start;
}


uint64_t Sinfo::get_sched_end(void)
{
	if (!check_magic())
		return 0;

	return sinfo->tsc_schedule_end;
}


void Sinfo::log_status()
{
	if (!sinfo) {
		Genode::log("Sinfo API not initialized");
		return;
	}
	if (!check_magic()) {
		Genode::log("Sinfo MAGIC not found");
		return;
	}

	Genode::log("muen-sinfo: Subject information exports ",
	            sinfo->memregion_count, " memory region(s)");
	for_each_memregion(log_memregion, 0);
	Genode::log("muen-sinfo: Subject information exports ",
	            sinfo->channel_info_count, " channel(s)");
	for_each_channel(log_channel, 0);
}


void Sinfo::fill_memregion_data(uint8_t idx, struct Memregion_info *region)
{
	const struct resource_type resource = sinfo->resources[idx];
	const struct memregion_type memregion =
		sinfo->memregions[resource.memregion_idx - 1];

	memset(&region->name, 0, MAX_NAME_LENGTH + 1);
	memcpy(&region->name, resource.name.data, resource.name.length);

	region->address    = memregion.address;
	region->size       = memregion.size;
	region->writable   = memregion.flags & MEM_WRITABLE_FLAG;
	region->executable = memregion.flags & MEM_EXECUTABLE_FLAG;
}


void Sinfo::fill_channel_data(uint8_t idx, struct Channel_info *channel)
{
	const struct resource_type resource = sinfo->resources[idx];
	const struct memregion_type memregion =
		sinfo->memregions[resource.memregion_idx - 1];
	const struct channel_info_type channel_info =
		sinfo->channels_info[resource.channel_info_idx - 1];

	memset(&channel->name, 0, MAX_NAME_LENGTH + 1);
	memcpy(&channel->name, resource.name.data, resource.name.length);

	channel->address  = memregion.address;
	channel->size     = memregion.size;
	channel->writable = memregion.flags & MEM_WRITABLE_FLAG;

	channel->has_event    = channel_info.flags & CHAN_EVENT_FLAG;
	channel->event_number = channel_info.event;
	channel->has_vector   = channel_info.flags & CHAN_VECTOR_FLAG;
	channel->vector       = channel_info.vector;
}


void Sinfo::fill_dev_data(uint8_t idx, struct Dev_info *dev)
{
	const struct dev_info_type dev_info = sinfo->dev_info[idx];

	dev->sid         = dev_info.sid;
	dev->irte_start  = dev_info.irte_start;
	dev->irq_start   = dev_info.irq_start;
	dev->ir_count    = dev_info.ir_count;
	dev->msi_capable = dev_info.flags & DEV_MSI_FLAG;
}
