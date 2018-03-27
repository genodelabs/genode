/*
 * \brief  Muen subject information API
 * \author Reto Buerki
 * \date   2015-04-21
 *
 * Defines functions to retrieve information about the execution environment of
 * a subject running on the Muen Separation Kernel.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64__MUEN__SINFO_H_
#define _INCLUDE__SPEC__X86_64__MUEN__SINFO_H_

#include <base/stdint.h>

struct Subject_info_type;
struct Scheduling_info_type;

namespace Genode
{
	/**
	 * Muen Subject Info class
	 */
	class Sinfo;
}

class Genode::Sinfo
{
	public:

		enum Config {
			MUEN_SUBJECT_INFO_MAGIC	= 0x02006f666e69756dULL,
			MAX_RESOURCE_COUNT		= 255,
			MAX_NAME_LENGTH    		= 63,
			PHYSICAL_BASE_ADDR		= 0xe00000000,
			SIZE               		= 0x8000,
			HASH_LENGTH        		= 32,
			MEM_WRITABLE_FLAG		= (1 << 0),
			MEM_EXECUTABLE_FLAG		= (1 << 1),
			MEM_CHANNEL_FLAG		= (1 << 2),
			DEV_MSI_FLAG			= (1 << 0),
		};

		Sinfo(const addr_t base_addr);

		/* Resource name */
		struct Name_type {
			uint8_t length;
			char data[MAX_NAME_LENGTH];
			uint8_t null_term;
		} __attribute__((packed));

		enum Content {
			CONTENT_UNINITIALIZED,
			CONTENT_FILL,
			CONTENT_FILE,
		};

		/* Structure holding information about a memory region */
		struct Memregion_type {
			enum Content content;
			uint64_t address;
			uint64_t size;
			uint8_t hash[HASH_LENGTH];
			uint8_t flags;
			uint16_t pattern;
			char padding[1];
		} __attribute__((packed));

		/*
		 * Structure holding information about a PCI device,
		 * explicitly padded to the size of the largest resource variant
		 */
		struct Device_type {
			uint16_t sid;
			uint16_t irte_start;
			uint8_t irq_start;
			uint8_t ir_count;
			uint8_t flags;
			char padding[sizeof(struct Memregion_type) - 7];
		} __attribute__((packed));

		/* Currently known resource types */
		enum Resource_kind {
			RES_NONE,
			RES_MEMORY,
			RES_EVENT,
			RES_VECTOR,
			RES_DEVICE
		};

		/* Resource data depending on the kind of resource */
		union Resource_data {
			struct Memregion_type mem;
			struct Device_type dev;
			uint8_t number;
		};

		/* Exported resource with associated name */
		struct Resource_type {
			enum Resource_kind kind;
			struct Name_type name;
			char padding[3];
			union Resource_data data;
		} __attribute__((packed));

		/* Muen subject information (sinfo) structure */
		struct Subject_info_type {
			uint64_t magic;
			uint32_t tsc_khz;
			struct Name_type name;
			uint16_t resource_count;
			char padding[1];
			struct Resource_type resources[MAX_RESOURCE_COUNT];
		} __attribute__((packed));

		/*
		 * Check Muen sinfo Magic.
		 */
		bool check_magic(void) const;

		/*
		 * Return subject name.
		 *
		 * The function returns NULL if the subject name cannot be retrieved.
		 */
		const char * get_subject_name(void);

		/*
		 * Return resource with given name and kind.
		 *
		 * If no resource with given name exists, null is returned.
		 */
		const struct Resource_type *
			get_resource(const char *const name, enum Resource_kind kind) const;

		/*
		 * Return information for PCI device with given SID.
		 *
		 * The function returns null if no device information for the specified device
		 * exists.
		 */
		const struct Device_type * get_device(const uint16_t sid) const;

		/*
		 * Resource callback.
		 *
		 * Used in the for_each_resource function. The optional void data
		 * pointer can be used to pass additional data.
		 */
		typedef bool (*resource_cb)(const struct Resource_type *const res,
				void *data);

		/*
		 * Invoke given callback function for each available resource.
		 *
		 * Resource information and the optional data argument are passed to each
		 * invocation of the callback. If a callback invocation returns false,
		 * processing is aborted and false is returned to the caller.
		 */
		bool for_each_resource(resource_cb func, void *data) const;

		/*
		 * Return TSC tick rate in kHz.
		 *
		 * The function returns 0 if the TSC tick rate cannot be retrieved.
		 */
		uint64_t get_tsc_khz(void) const;

		/*
		 * Return start time of current minor frame in TSC ticks.
		 */
		uint64_t get_sched_start(void) const;

		/*
		 * Return end time of current minor frame in TSC ticks.
		 */
		uint64_t get_sched_end(void) const;

		/*
		 * Log sinfo status.
		 */
		void log_status();

	private:

		Subject_info_type * sinfo = nullptr;
		Scheduling_info_type * sched_info = nullptr;
		char subject_name[MAX_NAME_LENGTH + 1];
		bool subject_name_set = false;

		/*
		 * Iterate over all resources beginning at given start resource.  If the res
		 * member of the iterator is nil, the function (re)starts the iteration at the first
		 * resource.
		 */
		struct iterator {
			const struct Resource_type *res;
			unsigned int idx;
		};

		bool iterate_resources(struct iterator *const iter) const;
};

#endif /* _INCLUDE__SPEC__X86_64__MUEN__SINFO_H_ */
