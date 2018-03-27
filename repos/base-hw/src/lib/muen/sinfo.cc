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

#include "muschedinfo.h"

#define roundup(x, y) (                           \
{                                                 \
	const typeof(y) __y = y;                        \
	(((x) + (__y - 1)) / __y) * __y;                \
})

using namespace Genode;

static_assert(sizeof(Sinfo::Subject_info_type) <= Sinfo::SIZE,
	 "Size of subject info type larger than Sinfo::SIZE.");


static const char * const content_names[] = {
	"uninitialized", "fill", "file",
};

uint8_t no_hash[Sinfo::HASH_LENGTH] = {0};

/* Return true if given buffer contains a hash */
static bool hash_available(const uint8_t * const first)
{
	return memcmp(first, no_hash, Sinfo::HASH_LENGTH) != 0;
}


static bool names_equal(const struct Sinfo::Name_type *const n1,
						const char *const n2)
{
	return n1->length == strlen(n2)	&& strcmp(n1->data, n2) == 0;
}


/* Convert given hash to hex string */
static char *hash_to_hex(char *buffer, const unsigned char *first)
{
	int i;
	for (i = 0; i < Sinfo::HASH_LENGTH; i++)
		snprintf(&buffer[i * 2], 3, "%02x", (unsigned int)*first++);
	return buffer;
}


static bool log_resource(const struct Sinfo::Resource_type *const res, void *)
{
	char hash_str[65];

	switch (res->kind) {
	case Sinfo::RES_MEMORY:
		Genode::log("muen-sinfo: memory [",
					content_names[res->data.mem.content],
					", addr ", Genode::Hex(res->data.mem.address),
					" size ", Genode::Hex(res->data.mem.size), " ",
					res->data.mem.flags & Sinfo::MEM_WRITABLE_FLAG ? "rw" : "ro",
					res->data.mem.flags & Sinfo::MEM_EXECUTABLE_FLAG ? "x" : "-",
					res->data.mem.flags & Sinfo::MEM_CHANNEL_FLAG ? "c" : "-",
					"] ", res->name.data);

		if (res->data.mem.content == Sinfo::CONTENT_FILL)
			Genode::log("muen-sinfo:  [pattern ", res->data.mem.pattern, "]");

		if (hash_available(res->data.mem.hash))
			Genode::log("muen-sinfo:  [hash 0x",
						Genode::Cstring(hash_to_hex(hash_str, res->data.mem.hash)),
						"]");
		break;
	case Sinfo::RES_DEVICE:
		Genode::log("muen-sinfo: device [sid ", Genode::Hex(res->data.dev.sid),
					" IRTE/IRQ start ", res->data.dev.irte_start,
					"/", res->data.dev.irq_start,
					" IR count ", res->data.dev.ir_count,
					" flags ", res->data.dev.flags, "] ", res->name.data);
		break;
	case Sinfo::RES_EVENT:
		Genode::log("muen-sinfo: event [number ", res->data.number, "] ", res->name.data);
		break;
	case Sinfo::RES_VECTOR:
		Genode::log("muen-sinfo: vector [number ", res->data.number, "] ", res->name.data);
		break;
	case Sinfo::RES_NONE:
		break;
	default:
		Genode::log("muen-sinfo: UNKNOWN resource at address %p\n",
			res);
		break;
	}

	return true;
}


Sinfo::Sinfo(const addr_t base_addr)
:
	sinfo((Subject_info_type *)base_addr)
{
	const uint64_t sinfo_page_size = roundup(sizeof(Subject_info_type), 0x1000);
	sched_info = ((Scheduling_info_type *)(base_addr + sinfo_page_size));

	if (!check_magic()) {
		Genode::error("muen-sinfo: Subject information MAGIC mismatch");
		return;
	}
}


bool Sinfo::check_magic(void) const
{
	return sinfo != 0 && sinfo->magic == MUEN_SUBJECT_INFO_MAGIC;
}


const char * Sinfo::get_subject_name(void)
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


bool Sinfo::iterate_resources(struct iterator *const iter) const
{
	if (!iter->res) {
		iter->res = &sinfo->resources[0];
		iter->idx = 0;
	} else {
		iter->res++;
		iter->idx++;
	}
	return iter->idx < sinfo->resource_count
		&& iter->res->kind != RES_NONE;
}


const struct Sinfo::Resource_type *
Sinfo::get_resource(const char *const name, enum Resource_kind kind) const
{
	struct iterator i = { nullptr, 0 };

	while (iterate_resources(&i))
		if (i.res->kind == kind && names_equal(&i.res->name, name))
			return i.res;

	return nullptr;
}


const struct Sinfo::Device_type * Sinfo::get_device(const uint16_t sid) const
{
	struct iterator i = { nullptr, 0 };

	while (iterate_resources(&i))
		if (i.res->kind == RES_DEVICE && i.res->data.dev.sid == sid)
			return &i.res->data.dev;

	return nullptr;
}


bool Sinfo::for_each_resource(resource_cb func, void *data) const
{
	struct iterator i = { nullptr, 0 };

	while (iterate_resources(&i))
		if (!func(i.res, data))
			return 0;

	return 1;
}


uint64_t Sinfo::get_tsc_khz(void) const
{
	if (!check_magic())
		return 0;

	return sinfo->tsc_khz;
}


uint64_t Sinfo::get_sched_start(void) const
{
	if (!check_magic())
		return 0;

	return sched_info->tsc_schedule_start;
}


uint64_t Sinfo::get_sched_end(void) const
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

	const uint16_t count = sinfo->resource_count;

	Genode::log("muen-sinfo: Subject name is '",
				Sinfo::get_subject_name(), "'");
	Genode::log("muen-sinfo: Subject exports ", count, " resources");
	for_each_resource(log_resource, nullptr);
}
