/*
 * \brief  Muen subject information API impl
 * \author Reto Buerki
 * \date   2015-04-21
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/snprintf.h>
#include <util/string.h>

#include <muen/sinfo.h>

#include "musinfo.h"
#include "muschedinfo.h"

#define roundup(x, y) (                           \
{                                                 \
	const typeof(y) __y = y;                        \
	(((x) + (__y - 1)) / __y) * __y;                \
})

static_assert(sizeof(subject_info_type) <= Sinfo::SIZE,
	 "Size of subject info type larger than Sinfo::SIZE.");

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


static const char * const content_names[] = {
	"uninitialized", "fill", "file",
};

uint8_t no_hash[Sinfo::HASH_LENGTH] = {0};

/* Return true if given buffer contains a hash */
static bool hash_available(const uint8_t * const first)
{
	return memcmp(first, no_hash, Sinfo::HASH_LENGTH) != 0;
}


/* Convert given hash to hex string */
static const char * const hash_to_hex(char *buffer, const unsigned char *first)
{
	int i;
	for (i = 0; i < Sinfo::HASH_LENGTH; i++)
		snprintf(&buffer[i * 2], 3, "%02x", (unsigned int)*first++);
	return buffer;
}


/* Log memory region information */
static bool log_memregion(const struct Genode::Sinfo::Memregion_info * const region,
                          void *data)
{
	char hash_str[65];

	Genode::log("muen-sinfo: [", content_names[region->content],
	            ", addr ", Genode::Hex(region->address),
	            " size ", Genode::Hex(region->size), " ",
	            region->writable ? "rw" : "ro",
	            region->executable ? "x" : "-",
	            "] ", region->name);

	if (region->content == Sinfo::CONTENT_FILL)
		Genode::log("muen-sinfo:  [pattern ", region->pattern, "]");
	if (hash_available(region->hash))
		Genode::log("muen-sinfo:  [hash 0x",
		            hash_to_hex(hash_str, region->hash), "]");

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
	const uint64_t sinfo_page_size = roundup(sizeof(subject_info_type), 0x1000);
	sinfo      = ((subject_info_type *)base_addr);
	sched_info = ((scheduling_info_type *)(base_addr + sinfo_page_size));

	if (!check_magic()) {
		Genode::error("muen-sinfo: Subject information MAGIC mismatch");
		return;
	}
}


bool Sinfo::check_magic(void)
{
	return sinfo != 0 && sinfo->magic == MUEN_SUBJECT_INFO_MAGIC;
}


const char * const Sinfo::get_subject_name(void)
{
	if (!check_magic())
		return nullptr;

	if (!subject_name_set)
	{
		memset(subject_name, 0, MAX_NAME_LENGTH + 1);
		memcpy(subject_name, &sinfo->name.data, sinfo->name.length);
		subject_name_set = true;
	}

	return subject_name;
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

	return sched_info->tsc_schedule_start;
}


uint64_t Sinfo::get_sched_end(void)
{
	if (!check_magic())
		return 0;

	return sched_info->tsc_schedule_end;
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

	Genode::log("muen-sinfo: Subject name is '",
				Sinfo::get_subject_name(), "'");
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

	memcpy(&region->hash, memregion.hash, HASH_LENGTH);

	region->content    = memregion.content;
	region->address    = memregion.address;
	region->size       = memregion.size;
	region->pattern    = memregion.pattern;
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
