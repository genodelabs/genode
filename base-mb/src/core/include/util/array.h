/*
 * \brief  Utils to ease the work with arrays
 * \author Martin stein
 * \date   2011-03-22
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__ARRAY_H_
#define _INCLUDE__UTIL__ARRAY_H_

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))
#define MAX_ARRAY_ID(array) (ARRAY_SIZE(array)-1)
#define LAST_ARRAY_ELEM(array) array[MAX_ARRAY_ID(array)]

#endif /* _INCLUDE__UTIL__ARRAY_H_ */
