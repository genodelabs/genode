/*
 * \brief  Replaces drivers/char/random.c
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/random.h>

void get_random_bytes(void * buf,int nbytes)
{
    lx_emul_trace(__func__);
}


int __must_check get_random_bytes_arch(void * buf,int nbytes)
{
    lx_emul_trace(__func__);
    return 0;
}
