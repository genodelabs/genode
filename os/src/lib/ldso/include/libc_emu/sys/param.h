/*
 * \brief  param.h prototypes/definitions required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef PARAM_H
#define PARAM_H

//XXX cbass: hard code for now
#define PAGE_SIZE 0x1000
#define PAGE_MASK ~(PAGE_SIZE - 1)
#define PATH_MAX 1024
#define MAXPATHLEN PATH_MAX

static inline 
unsigned long round_page(unsigned long page) 
{
	return ((page + PAGE_SIZE - 1) & PAGE_MASK);
}

static inline
unsigned long trunc_page(unsigned long page)
{
	return (page & PAGE_MASK);
}

#endif //PARAM_H
