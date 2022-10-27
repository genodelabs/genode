/*
 * \brief  Enforced global definitions
 * \author Christian Helmuth
 * \date   2020-11-25
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef ___global_defs_h___
#define ___global_defs_h___

#ifdef __cplusplus

/*
 * Prevent inclusion of the Genode::Log definition after the vbox #define
 * of 'Log'. Otherwise, the attempt to compile base/log.h will fail.
 */
#include <base/log.h>

/*
 * Place hook to return current log configuration to SVC main initialization.
 */
#define VBOXSVC_LOG_DEFAULT vboxsvc_log_default_string()

extern "C" char const * vboxsvc_log_default_string();

/* missing glibc function */
extern "C" void *mempcpy(void *dest, const void *src, unsigned long n);

#endif /* __cplusplus */

#endif /* ___global_defs_h___ */
