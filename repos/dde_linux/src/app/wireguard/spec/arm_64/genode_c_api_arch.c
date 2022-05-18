
#include <linux/netdevice.h>

void genode_wg_arch_net_dev_init(struct net_device *net_dev,
                                 int               *pcpu_refcnt)
{
	net_dev->pcpu_refcnt = pcpu_refcnt;
}


void finalize_system_capabilities(void);


void genode_wg_arch_lx_user_init(void)
{
	finalize_system_capabilities();
}
