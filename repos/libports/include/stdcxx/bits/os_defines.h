/*
 * \brief  Genode-specific defines
 * \author Christian Prochaska
 * \date   2025-03-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__STDCXX__BITS__OS_DEFINES_H_
#define _INCLUDE__STDCXX__BITS__OS_DEFINES_H_

/* see config/os/generic/os_defines.h */
#define _GLIBCXX_GTHREAD_USE_WEAK 0

/* prevent gcc headers from defining mbstate */
#define _GLIBCXX_HAVE_MBSTATE_T 1

/* use compiler-builtin atomic operations */
#define _GLIBCXX_ATOMIC_BUILTINS 1

/* no isinf isnan */
#define _GLIBCXX_NO_OBSOLETE_ISINF_ISNAN_DYNAMIC 1

/*
 * Use 'pthread_mutex_init()' and 'pthread_mutex_destroy()'
 * instead of 'PTHREAD_MUTEX_INITIALIZER' to avoid memory leaks.
 */
#define _GTHREAD_USE_MUTEX_INIT_FUNC 1

#endif /* _INCLUDE__STDCXX__BITS__OS_DEFINES_H_ */
