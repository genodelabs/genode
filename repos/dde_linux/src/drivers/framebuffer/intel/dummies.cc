/*
 * \brief  Dummy functions
 * \author Norman Feske
 * \date   2015-08-18
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "lx_emul_private.h"

extern "C" {

#include <drm/drmP.h>
#include <i915/i915_drv.h>

/*
 * Incorporate dummy implemementations semi-automaticall generated via the
 * 'gen_dummy' script.
 */

#include "gen_dummies.h"

/*
 * Manually defined dummies (because they are not covered by the heuristics
 * of the 'gen_dummy' script).
 */

struct timespec timespec_sub(struct timespec lhs, struct timespec rhs)
{
	TRACE_AND_STOP;
	return { 0, 0 };
}

struct timespec ns_to_timespec(const s64 nsec)
{
	TRACE_AND_STOP;
	return { 0, 0 };
}

void hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize, char *linebuf, size_t linebuflen, bool ascii)
{
	TRACE_AND_STOP;
}

bool mod_delayed_work(struct workqueue_struct *, struct delayed_work *, unsigned long)
{
	TRACE_AND_STOP;
	return false;
}

bool capable(int cap)
{
	TRACE_AND_STOP;
	return false;
}

int drm_dp_bw_code_to_link_rate(u8 link_bw)
{
	TRACE_AND_STOP;
	return -1;
}

bool intel_fbc_enabled(struct drm_device *dev)
{
	TRACE_AND_STOP;
	return false;
}

int i2c_dp_aux_add_bus(struct i2c_adapter *adapter)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_execbuffer(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_execbuffer2(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_set_tiling(struct drm_device *dev, void *data, struct drm_file *file)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_get_tiling(struct drm_device *dev, void *data, struct drm_file *file)
{
	TRACE_AND_STOP;
	return -1;
}

int intel_overlay_put_image(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	TRACE_AND_STOP;
	return -1;
}

int intel_overlay_attrs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	TRACE_AND_STOP;
	return -1;
}

bool flush_delayed_work(struct delayed_work *dwork)
{
	TRACE_AND_STOP;
	return false;
}

void *krealloc(const void *, size_t, gfp_t)
{
	TRACE_AND_STOP;
	return NULL;
}

void down_read(struct rw_semaphore *sem)
{
	TRACE_AND_STOP;
}

void device_unregister(struct device *dev)
{
	TRACE_AND_STOP;
}


} /* extern "C" */
