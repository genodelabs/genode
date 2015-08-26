#ifndef MUSINFO_H_
#define MUSINFO_H_

#include <base/stdint.h>

#define MUEN_SUBJECT_INFO_MAGIC	0x01006f666e69756dULL
#define MAX_NAME_LENGTH		63
#define MAX_RESOURCE_COUNT	255
#define NO_RESOURCE			0

using namespace Genode;

struct name_type {
	uint8_t length;
	char data[MAX_NAME_LENGTH];
} __attribute__((packed));

#define MEM_WRITABLE_FLAG   (1 << 0)
#define MEM_EXECUTABLE_FLAG (1 << 1)

struct memregion_type {
	uint64_t address;
	uint64_t size;
	uint8_t flags;
	char padding[7];
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

struct subject_info_type {
	uint64_t magic;
	uint8_t resource_count;
	uint8_t memregion_count;
	uint8_t channel_info_count;
	char padding[5];
	uint64_t tsc_khz;
	uint64_t tsc_schedule_start;
	uint64_t tsc_schedule_end;
	struct resource_type resources[MAX_RESOURCE_COUNT];
	struct memregion_type memregions[MAX_RESOURCE_COUNT];
	struct channel_info_type channels_info[MAX_RESOURCE_COUNT];
} __attribute__((packed, aligned (8)));

#endif /* MUSINFO_H_  */
