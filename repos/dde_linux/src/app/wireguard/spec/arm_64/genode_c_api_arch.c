
#include <linux/netdevice.h>

void finalize_system_capabilities(void);


void genode_wg_arch_lx_user_init(void)
{
	finalize_system_capabilities();
}
