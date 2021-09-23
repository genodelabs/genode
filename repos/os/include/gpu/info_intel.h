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

	Chip_id    chip_id;
	Features   features;
	size_t     aperture_size;
	Context_id ctx_id;

	Sequence_number last_completed;

	struct Revision      { Genode::uint8_t value; } revision;
	struct Slice_mask    { unsigned value; }        slice_mask;
	struct Subslice_mask { unsigned value; }        subslice_mask;
	struct Eu_total      { unsigned value; }        eus;
	struct Subslices     { unsigned value; }        subslices;

	Info_intel(Chip_id chip_id, Features features, size_t aperture_size,
	           Context_id ctx_id, Sequence_number last,
	           Revision rev, Slice_mask s_mask, Subslice_mask ss_mask,
	           Eu_total eu, Subslices subslice)
	:
		chip_id(chip_id), features(features),
		aperture_size(aperture_size), ctx_id(ctx_id),
		last_completed(last),
		revision(rev),
		slice_mask(s_mask),
		subslice_mask(ss_mask),
		eus(eu),
		subslices(subslice)
	{ }
};

#endif /* _INCLUDE__GPU_INFO_INTEL_H_ */
