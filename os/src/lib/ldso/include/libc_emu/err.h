/*
 * \brief  err.h prototypes required by ldso
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ERR_H_
#define _ERR_H_


#ifdef __cplusplus
extern "C" {
#endif

#define err errx
void errx(int eval, const char *fmt, ...) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
