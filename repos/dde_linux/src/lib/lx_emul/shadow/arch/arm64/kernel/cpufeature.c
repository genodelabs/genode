/*
 * \brief  Replaces arch/arm64/kernel/cpufeature.c
 * \author Martin Stein
 * \author Christian Helmuth
 * \date   2022-05-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/jump_label.h>
#include <linux/types.h>
#include <asm/cpufeature.h>

/*
 * Flag to indicate if we have computed the system wide
 * capabilities based on the boot time active CPUs. This
 * will be used to determine if a new booting CPU should
 * go through the verification process to make sure that it
 * supports the system capabilities, without using a hotplug
 * notifier. This is also used to decide if we could use
 * the fast path for checking constant CPU caps.
 */
DEFINE_STATIC_KEY_FALSE(arm64_const_caps_ready);
EXPORT_SYMBOL(arm64_const_caps_ready);
void finalize_system_capabilities(void)
{
	static_branch_enable(&arm64_const_caps_ready);
}

DEFINE_STATIC_KEY_ARRAY_FALSE(cpu_hwcap_keys, ARM64_NCAPS);
EXPORT_SYMBOL(cpu_hwcap_keys);

DECLARE_BITMAP(cpu_hwcaps, ARM64_NCAPS);
EXPORT_SYMBOL(cpu_hwcaps);

bool arm64_use_ng_mappings = false;
