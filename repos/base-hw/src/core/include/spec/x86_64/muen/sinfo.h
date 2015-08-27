/*
 * \brief  Muen subject information API
 * \author Reto Buerki
 * \date   2015-04-21
 *
 * Defines functions to retrieve information about the execution environment of
 * a subject running on the Muen Separation Kernel.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SINFO_H
#define _SINFO_H

#include <base/stdint.h>

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
			MAX_NAME_LENGTH = 63,
		};

		Sinfo();

		/* Structure holding information about a memory region */
		struct Memregion_info {
			char name[MAX_NAME_LENGTH + 1];
			uint64_t address;
			uint64_t size;
			bool writable;
			bool executable;
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

		/*
		 * Check Muen sinfo Magic.
		 */
		static bool check_magic(void);

		/*
		 * Return information for a channel given by name.
		 *
		 * If no channel with given name exists, False is returned. The
		 * event_number and vector parameters are only valid if indicated by
		 * the has_[event|vector] struct members.
		 */
		static bool get_channel_info(const char * const name,
				struct Channel_info *channel);

		/*
		 * Return information for a memory region given by name.
		 *
		 * If no memory region with given name exists, False is returned.
		 */
		static bool get_memregion_info(const char * const name,
				struct Memregion_info *memregion);

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
		static bool for_each_channel(Channel_cb func, void *data);

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
		static bool for_each_memregion(Memregion_cb func, void *data);

		/*
		 * Return TSC tick rate in kHz.
		 *
		 * The function returns 0 if the TSC tick rate cannot be retrieved.
		 */
		static uint64_t get_tsc_khz(void);

		/*
		 * Return start time of current minor frame in TSC ticks.
		 */
		static uint64_t get_sched_start(void);

		/*
		 * Return end time of current minor frame in TSC ticks.
		 */
		static uint64_t get_sched_end(void);
};

#endif /* _SINFO_H */
