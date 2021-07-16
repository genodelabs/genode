/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/********************
 ** linux/poison.h **
 ********************/

/*
 * In list.h, LIST_POISON1 and LIST_POISON2 are assigned to 'struct list_head
 * *' as well as 'struct hlist_node *' pointers. Consequently, C++ compiler
 * produces an error "cannot convert... in assignment". To compile 'list.h'
 * included by C++ source code, we have to define these macros to the only
 * value universally accepted for pointer assigments.h
 */

#ifdef __cplusplus
#define LIST_POISON1 nullptr
#define LIST_POISON2 nullptr
#else
#define LIST_POISON1 ((void *)0x00000100)
#define LIST_POISON2 ((void *)0x00000200)
#endif  /* __cplusplus */

#define	POISON_INUSE 0x5a

/******************
 ** linux/list.h **
 ******************/

#include <linux/list.h>
