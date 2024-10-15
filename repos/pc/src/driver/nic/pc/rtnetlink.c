/*
 * \brief  Basic RTNETLINK implementation with lock/unlock and netdev_run_todo()
 * \author Christian Helmuth
 * \date   2024-03-26
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/nic.h>

#include <net/rtnetlink.h>
#include <linux/mutex.h>

#include <../net/core/dev.h>


static DEFINE_MUTEX(rtnl_mutex);

void rtnl_register(int protocol,int msgtype,rtnl_doit_func doit,rtnl_dumpit_func dumpit,unsigned int flags)
{
}


int rtnl_lock_killable(void)
{
	return mutex_lock_killable(&rtnl_mutex);
}


int rtnl_is_locked(void)
{
	return mutex_is_locked(&rtnl_mutex);
}


void rtnl_lock(void)
{
	mutex_lock(&rtnl_mutex);
}


void __rtnl_unlock(void)
{
	WARN_ON(!list_empty(&net_todo_list));

	mutex_unlock(&rtnl_mutex);
}


void rtnl_unlock(void)
{
	netdev_run_todo();
}


/*
 * Called whenever the link state changes
 */
void rtmsg_ifinfo(int type, struct net_device * dev, unsigned int change, gfp_t flags,
                  u32 portid, const struct nlmsghdr *nlh)
{
	lx_emul_nic_handle_io();
}
