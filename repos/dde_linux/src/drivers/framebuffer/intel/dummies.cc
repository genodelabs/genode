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

bool capable(int cap)
{
	TRACE_AND_STOP;
	return false;
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

bool flush_delayed_work(struct delayed_work *dwork)
{
	TRACE_AND_STOP;
	return false;
}

void down_read(struct rw_semaphore *sem)
{
	TRACE_AND_STOP;
}

void device_unregister(struct device *dev)
{
	TRACE_AND_STOP;
}

int i2c_algo_bit_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	TRACE_AND_STOP;
	return -1;
}

u32 i2c_algo_bit_func(struct i2c_adapter *adap)
{
	TRACE_AND_STOP;
	return 0;
}

void i915_setup_sysfs(struct drm_device *dev_priv)
{
	TRACE;
}

int acpi_video_register(void)
{
	TRACE;
	return 0;
}

void ips_link_to_i915_driver(void)
{
	TRACE;
}

void spin_lock(spinlock_t *lock)
{
	TRACE;
}

void drm_sysfs_hotplug_event(struct drm_device *dev)
{
	TRACE;
}

const char *acpi_dev_name(struct acpi_device *)
{
	TRACE_AND_STOP;
	return 0;
}

} /* extern "C" */

