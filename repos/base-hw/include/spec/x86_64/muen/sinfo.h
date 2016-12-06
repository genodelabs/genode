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

struct subject_info_type;
struct scheduling_info_type;

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
			PHYSICAL_BASE_ADDR = 0xe00000000,
			SIZE               = 0x9000,
			MAX_NAME_LENGTH    = 63,
			HASH_LENGTH        = 32,
		};

		Sinfo(const addr_t base_addr);

		enum Content {
			CONTENT_UNINITIALIZED,
			CONTENT_FILL,
			CONTENT_FILE,
		};

		/* Structure holding information about a memory region */
		struct Memregion_info {
			enum Content content;
			char name[MAX_NAME_LENGTH + 1];
			uint64_t address;
			uint64_t size;
			bool writable;
			bool executable;
			uint8_t hash[HASH_LENGTH];
			uint16_t pattern;
		};

		/* Structure holding information about a Muen channel */
		struct Channel_info {
			char name[MAX_NAME_LENGTH + 1];
			uint64_t address;
			uint64_t size;
			uint8_t event_number;
			uint8_t vector;
			bool writable;
			bool has_event;
			bool has_vector;
		};

		/* Structure holding information about PCI devices */
		struct Dev_info {
			uint16_t sid;
			uint16_t irte_start;
			uint8_t irq_start;
			uint8_t ir_count;
			bool msi_capable;
		};

		/*
		 * Check Muen sinfo Magic.
		 */
		bool check_magic(void);

		/*
		 * Return subject name.
		 *
		 * The function returns NULL if the subject name cannot be retrieved.
		 */
		const char * const get_subject_name(void);

		/*
		 * Return information for a channel given by name.
		 *
		 * If no channel with given name exists, False is returned. The
		 * event_number and vector parameters are only valid if indicated by
		 * the has_[event|vector] struct members.
		 */
		bool get_channel_info(const char * const name,
				struct Channel_info *channel);

		/*
		 * Return information for a memory region given by name.
		 *
		 * If no memory region with given name exists, False is returned.
		 */
		bool get_memregion_info(const char * const name,
				struct Memregion_info *memregion);

		/*
		 * Return information for PCI device with given SID.
		 *
		 * The function returns false if no device information for the
		 * specified device exists.
		 */
		bool get_dev_info(const uint16_t sid, struct Dev_info *dev);

		/*
		 * Channel callback.
		 *
		 * Used in the muen_for_each_channel function. The optional void data pointer
		 * can be used to pass additional data.
		 */
		typedef bool (*Channel_cb)(const struct Channel_info * const channel,
				void *data);

		/*
		 * Invoke given callback function for each available channel.
		 *
		 * Channel information and the optional data argument are passed to each
		 * invocation of the callback. If a callback invocation returns false,
		 * processing is aborted and false is returned to the caller.
		 */
		bool for_each_channel(Channel_cb func, void *data);

		/*
		 * Memory region callback.
		 *
		 * Used in the muen_for_each_memregion function. The optional void data pointer
		 * can be used to pass additional data.
		 */
		typedef bool (*Memregion_cb)(const struct Memregion_info * const memregion,
				void *data);

		/*
		 * Invoke given callback function for each available memory region.
		 *
		 * Memory region information and the optional data argument are passed to each
		 * invocation of the callback. If a callback invocation returns false,
		 * processing is aborted and false is returned to the caller.
		 */
		bool for_each_memregion(Memregion_cb func, void *data);

		/*
		 * Return TSC tick rate in kHz.
		 *
		 * The function returns 0 if the TSC tick rate cannot be retrieved.
		 */
		uint64_t get_tsc_khz(void);

		/*
		 * Return start time of current minor frame in TSC ticks.
		 */
		uint64_t get_sched_start(void);

		/*
		 * Return end time of current minor frame in TSC ticks.
		 */
		uint64_t get_sched_end(void);

		/*
		 * Log sinfo status.
		 */
		void log_status();

	private:

		subject_info_type * sinfo;
		scheduling_info_type * sched_info;
		char subject_name[MAX_NAME_LENGTH + 1];
		bool subject_name_set = false;

		/*
		 * Fill memregion struct with memory region info from resource given by
		 * index.
		 */
		void fill_memregion_data(uint8_t idx, struct Memregion_info *region);

		/*
		 * Fill channel struct with channel information from resource given by
		 * index.
		 */
		void fill_channel_data(uint8_t idx, struct Channel_info *channel);

		/* Fill dev struct with data from PCI device info given by index. */
		void fill_dev_data(uint8_t idx, struct Dev_info *dev);
};

#endif /* _INCLUDE__SPEC__X86_64__MUEN__SINFO_H_ */
