/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Needed to trace and stop */
#include <lx_emul/debug.h>

/* fix for wait_for_completion_timeout where the __sched include is missing */
#include <linux/sched/debug.h>

/* fix for missing include in linux/dynamic_debug.h */
#include <linux/compiler_attributes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* forward declarations for generated_dummies.c */
struct ieee80211_local;
struct iwl_priv;
struct iwl_mvm;

int  lx_emul_rfkill_get_any(void);
void lx_emul_rfkill_switch_all(int blocked);

#ifdef __cplusplus
}
#endif
