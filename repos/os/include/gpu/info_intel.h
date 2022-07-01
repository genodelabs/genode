/*
 * \brief  Gpu session interface.
 * \author Josef Soentgen
 * \date   2021-09-23
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_INFO_INTEL_H_
#define _INCLUDE__GPU_INFO_INTEL_H_

#include <gpu_session/gpu_session.h>

namespace Gpu {

	struct Info_intel;
}


/*
 * Gpu information
 *
 * Used to query information in the DRM backend
 */
struct Gpu::Info_intel
{
	using Chip_id    = Genode::uint16_t;
	using Features   = Genode::uint32_t;
	using size_t     = Genode::size_t;
	using Context_id = Genode::uint32_t;
	using uint8_t    = Genode::uint8_t;

	Chip_id    chip_id;
	Features   features;
	size_t     aperture_size;
	Context_id ctx_id;

	Sequence_number last_completed;

	struct Revision        { Genode::uint8_t value; } revision;
	struct Slice_mask      { unsigned value; }        slice_mask;
	struct Subslice_mask   { unsigned value; }        subslice_mask;
	struct Eu_total        { unsigned value; }        eus;
	struct Subslices       { unsigned value; }        subslices;
	struct Clock_frequency { unsigned value; }        clock_frequency;

	struct Topology
	{
		enum {
			MAX_SLICES = 3,
			MAX_SUBSLICES = 32,
			MAX_EUS = 16,
		};

		uint8_t slice_mask { 0 };
		uint8_t subslice_mask[MAX_SLICES * (MAX_SUBSLICES / 8)] { };
		uint8_t eu_mask[MAX_SLICES * MAX_SUBSLICES * (MAX_EUS / 8)] { };

		uint8_t max_slices { 0 };
		uint8_t max_subslices { 0 };
		uint8_t max_eus_per_subslice { 0 };

		uint8_t ss_stride { 0 };
		uint8_t eu_stride { 0 };

		bool valid { false };

		bool has_subslice(unsigned slice, unsigned subslice)
		{
			unsigned ss_idx = subslice / 8;
			uint8_t mask = subslice_mask[slice * ss_stride + ss_idx];
			return mask & (1u << (subslice % 8));
		}

		unsigned eu_idx(unsigned slice, unsigned subslice)
		{
			unsigned slice_stride = max_slices * eu_stride;
			return slice * slice_stride + subslice * eu_stride;
		}
	};

	struct Topology topology { };

	Info_intel(Chip_id chip_id, Features features, size_t aperture_size,
	           Context_id ctx_id, Sequence_number last,
	           Revision rev, Slice_mask s_mask, Subslice_mask ss_mask,
	           Eu_total eu, Subslices subslice, Clock_frequency clock_frequency,
	           Topology topology)
	:
		chip_id(chip_id), features(features),
		aperture_size(aperture_size), ctx_id(ctx_id),
		last_completed(last),
		revision(rev),
		slice_mask(s_mask),
		subslice_mask(ss_mask),
		eus(eu),
		subslices(subslice),
		clock_frequency(clock_frequency),
		topology(topology)
	{ }
};

#endif /* _INCLUDE__GPU_INFO_INTEL_H_ */
