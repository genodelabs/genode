/*
 * \brief  Dummies that cannot be implemented with the DUMMY macros
 * \author Sebastian Sumpf
 * \date   2014-02-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* local includes */
#include <lx_emul.h>


int security_secid_to_secctx(u32 secid, char **secdata, u32 *seclen)
{
		printk("%s called (from %p) not implemented", __func__, __builtin_return_address(0));
	
	*seclen = 0;
	return -1;
}


int ip_mc_msfget(struct sock *sk, struct ip_msfilter *msf,
                 struct ip_msfilter *optval, int *optlen)
{
	printk("%s called (from %p) not implemented", __func__, __builtin_return_address(0));
	
	*optlen = 0;
	return -1;
}

int ip_mc_gsfget(struct sock *sk, struct group_filter *gsf,
                 struct group_filter  *optval, int  *optlen)
{
	printk("%s called (from %p) not implemented", __func__, __builtin_return_address(0));

	*optlen = 0;
	return -1;
}
