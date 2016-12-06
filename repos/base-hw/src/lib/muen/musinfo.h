/*
 * \brief   Muen subject info
 * \author  Reto Buerki
 * \date    2015-04-21
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__MUEN__MUSINFO_H_
#define _LIB__MUEN__MUSINFO_H_

#include <base/stdint.h>
#include <muen/sinfo.h>

#define MUEN_SUBJECT_INFO_MAGIC	0x01006f666e69756dULL

#define MAX_RESOURCE_COUNT	255
#define NO_RESOURCE			0

using namespace Genode;

struct name_type {
	uint8_t length;
	char data[Sinfo::MAX_NAME_LENGTH];
} __attribute__((packed));

#define MEM_WRITABLE_FLAG   (1 << 0)
#define MEM_EXECUTABLE_FLAG (1 << 1)

struct memregion_type {
	enum Sinfo::Content content;
	uint64_t address;
	uint64_t size;
	uint8_t hash[Sinfo::HASH_LENGTH];
	uint8_t flags;
	uint16_t pattern;
	char padding[1];
} __attribute__((packed, aligned (8)));

#define CHAN_EVENT_FLAG  (1 << 0)
#define CHAN_VECTOR_FLAG (1 << 1)

struct channel_info_type {
	uint8_t flags;
	uint8_t event;
	uint8_t vector;
	char padding[5];
} __attribute__((packed, aligned (8)));

struct resource_type {
	struct name_type name;
	uint8_t memregion_idx;
	uint8_t channel_info_idx;
	char padding[6];
} __attribute__((packed, aligned (8)));

struct dev_info_type {
	uint16_t sid;
	uint16_t irte_start;
	uint8_t irq_start;
	uint8_t ir_count;
	uint8_t flags;
	char padding[1];
} __attribute__((packed, aligned (8)));

#define DEV_MSI_FLAG  (1 << 0)

struct subject_info_type {
	uint64_t magic;
	struct name_type name;
	uint8_t resource_count;
	uint8_t memregion_count;
	uint8_t channel_info_count;
	uint8_t dev_info_count;
	char padding[4];
	uint64_t tsc_khz;
	struct resource_type resources[MAX_RESOURCE_COUNT];
	struct memregion_type memregions[MAX_RESOURCE_COUNT];
	struct channel_info_type channels_info[MAX_RESOURCE_COUNT];
	struct dev_info_type dev_info[MAX_RESOURCE_COUNT];
} __attribute__((packed, aligned (8)));

#endif /* _LIB__MUEN__MUSINFO_H_ */
