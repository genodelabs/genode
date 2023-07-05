#include <lx_emul.h>

#if 0
#define TRACE \
	do { \
		lx_printf("%s not implemented\n", __func__); \
	} while (0)
#else
#define TRACE do { ; } while (0)
#endif

#define TRACE_AND_STOP \
	do { \
		lx_printf("%s not implemented\n", __func__); \
		BUG(); \
	} while (0)

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	TRACE_AND_STOP;
	return NULL;
}

u16 bitrev16(u16 in)
{
	TRACE_AND_STOP;
	return -1;
}

u16 crc16(u16 crc, const u8 *buffer, size_t len)
{
	TRACE_AND_STOP;
	return -1;
}

__sum16 csum_fold(__wsum csum)
{
	TRACE_AND_STOP;
	return -1;
}

__wsum csum_partial(const void *buff, int len, __wsum sum)
{
	TRACE_AND_STOP;
	return -1;
}

int device_set_wakeup_enable(struct device *dev, bool enable)
{
	TRACE_AND_STOP;
	return -1;
}

void dst_release(struct dst_entry *dst)
{
	TRACE_AND_STOP;
}

struct net_device;
u32 ethtool_op_get_link(struct net_device *dev)
{
	TRACE_AND_STOP;
	return -1;
}

int ethtool_op_get_ts_info(struct net_device *dev, struct ethtool_ts_info *eti)
{
	TRACE_AND_STOP;
	return -1;
}

bool file_ns_capable(const struct file *file, struct user_namespace *ns, int cap)
{
	TRACE_AND_STOP;
	return -1;
}

void free_uid(struct user_struct * user)
{
	TRACE_AND_STOP;
}

struct mii_if_info;
int generic_mii_ioctl(struct mii_if_info *mii_if, struct mii_ioctl_data *mii_data, int cmd, unsigned int *duplex_changed)
{
	TRACE_AND_STOP;
	return -1;
}

void get_page(struct page *page)
{
	TRACE_AND_STOP;
}

bool gfpflags_allow_blocking(const gfp_t gfp_flags)
{
	TRACE_AND_STOP;
	return -1;
}

bool gfp_pfmemalloc_allowed(gfp_t gfp_flags)
{
	TRACE_AND_STOP;
	return -1;
}

int in_irq()
{
	TRACE_AND_STOP;
	return -1;
}

void *kmap_atomic(struct page *page)
{
	TRACE_AND_STOP;
	return NULL;
}

int memcmp(const void *s1, const void *s2, size_t size)
{
	TRACE_AND_STOP;
	return -1;
}

void mii_ethtool_get_link_ksettings( struct mii_if_info *mii, struct ethtool_link_ksettings *cmd)
{
	TRACE_AND_STOP;
}

int mii_ethtool_set_link_ksettings( struct mii_if_info *mii, const struct ethtool_link_ksettings *cmd)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned netdev_mc_count(struct net_device * dev)
{
	TRACE;
	return 1;
}

int netdev_mc_empty(struct net_device * ndev)
{
	TRACE;
	return 1;
}

void netdev_stats_to_stats64(struct rtnl_link_stats64 *stats64, const struct net_device_stats *netdev_stats)
{
	TRACE_AND_STOP;
}

bool netdev_uses_dsa(struct net_device *dev)
{
	TRACE;
	return false;
}

void netif_device_attach(struct net_device *dev)
{
	TRACE;
}

void netif_device_detach(struct net_device *dev)
{
	TRACE_AND_STOP;
}

int  netif_device_present(struct net_device * d)
{
	TRACE;
	return 1;
}

u32 netif_msg_init(int arg0, int arg1)
{
	TRACE;
	return 0;
}

void netif_start_queue(struct net_device *dev)
{
	TRACE;
}

void netif_stop_queue(struct net_device *dev)
{
	TRACE;
}

void netif_trans_update(struct net_device *dev)
{
	TRACE;
}

void netif_tx_wake_all_queues(struct net_device *dev)
{
	TRACE_AND_STOP;
}

void netif_wake_queue(struct net_device * d)
{
	TRACE;
}

const void *of_get_mac_address(struct device_node *np)
{
	TRACE;
	return NULL;
}

void pm_runtime_enable(struct device *dev)
{
	TRACE;
}

void read_lock_bh(rwlock_t * l)
{
	TRACE_AND_STOP;
}

void read_unlock_bh(rwlock_t * l)
{
	TRACE_AND_STOP;
}

void secpath_reset(struct sk_buff * skb)
{
	TRACE;
}

void __set_current_state(int state)
{
	TRACE;
}

void sg_init_table(struct scatterlist *sg, unsigned int arg)
{
	TRACE_AND_STOP;
}

void sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen)
{
	TRACE_AND_STOP;
}

void sg_set_page(struct scatterlist *sg, struct page *page, unsigned int len, unsigned int offset)
{
	TRACE_AND_STOP;
}

void sk_free(struct sock *sk)
{
	TRACE_AND_STOP;
}

void sock_efree(struct sk_buff *skb)
{
	TRACE_AND_STOP;
}

void spin_lock_irq(spinlock_t *lock)
{
	TRACE_AND_STOP;
}

void spin_lock_nested(spinlock_t *lock, int subclass)
{
	TRACE;
}

void spin_unlock_irq(spinlock_t *lock)
{
	TRACE_AND_STOP;
}

size_t strlcpy(char *dest, const char *src, size_t size)
{
	TRACE_AND_STOP;
	return -1;
}

void tasklet_kill(struct tasklet_struct *t)
{
	TRACE;
}

void trace_consume_skb(struct sk_buff *skb)
{
	TRACE;
}

void trace_kfree_skb(struct sk_buff *skb, void *arg)
{
	TRACE;
}

unsigned int u64_stats_fetch_begin_irq(const struct u64_stats_sync *p)
{
	TRACE_AND_STOP;
	return -1;
}

bool u64_stats_fetch_retry_irq(const struct u64_stats_sync *p, unsigned int s)
{
	TRACE_AND_STOP;
	return -1;
}

struct usb_device;
int usb_clear_halt(struct usb_device *dev, int pipe)
{
	TRACE_AND_STOP;
	return -1;
}

struct usb_driver;
struct usb_interface;
void usb_driver_release_interface(struct usb_driver *driver, struct usb_interface *iface)
{
	TRACE;
}


/* only called to kill interrupt urb in usbnet.c */
struct urb;
void usb_kill_urb(struct urb *urb)
{
	TRACE;
}


struct usb_anchor;
struct urb *usb_get_from_anchor(struct usb_anchor *anchor)
{
	TRACE_AND_STOP;
	return NULL;
}

int usb_interrupt_msg(struct usb_device *usb_dev, unsigned int pipe, void *data, int len, int *actual_length, int timeout)
{
	TRACE_AND_STOP;
	return -1;
}

void usb_scuttle_anchored_urbs(struct usb_anchor *anchor)
{
	TRACE;
}

ktime_t ktime_get_real(void)
{
	TRACE_AND_STOP;
	return -1;
}

void kunmap_atomic(void *addr)
{
	TRACE_AND_STOP;
}

void might_sleep()
{
	TRACE_AND_STOP;
}

bool page_is_pfmemalloc(struct page *page)
{
	TRACE_AND_STOP;
	return -1;
}

void put_page(struct page *page)
{
	TRACE_AND_STOP;
}

int usb_unlink_urb(struct urb *urb)
{
	TRACE;
	return 0;
}


struct urb *usb_get_urb(struct urb *urb)
{
	TRACE;
	return NULL;
}


void usleep_range(unsigned long min, unsigned long max)
{
	TRACE;
}

void phy_stop(struct phy_device *phydev)
{
	TRACE;
}

void phy_disconnect(struct phy_device *phydev)
{
	TRACE;
}

void phy_print_status(struct phy_device *phydev)
{
	TRACE;
}

int phy_mii_ioctl(struct phy_device *phydev, struct ifreq *ifr, int cmd)
{
	TRACE;
	return 0;
}

typedef int phy_interface_t;
struct phy_device * phy_connect(struct net_device *dev, const char *bus_id, void (*handler)(struct net_device *), phy_interface_t interface)
{
	TRACE;
	return 0;
}

int  genphy_resume(struct phy_device *phydev) { TRACE; return 0; }

void phy_start(struct phy_device *phydev) { TRACE; }

struct mii_bus;
void mdiobus_free(struct mii_bus *bus) { TRACE; }

void mdiobus_unregister(struct mii_bus *bus) { TRACE; }

struct mii_bus *mdiobus_alloc_size(size_t size)
{
	TRACE_AND_STOP;
	return NULL;
}

int __mdiobus_register(struct mii_bus *bus, struct module *owner)
{
	TRACE_AND_STOP;
	return -1;
}

int phy_ethtool_get_link_ksettings(struct net_device *ndev, struct ethtool_link_ksettings *cmd)
{
	TRACE_AND_STOP;
	return -1;
}

int phy_ethtool_set_link_ksettings(struct net_device *ndev, const struct ethtool_link_ksettings *cmd)
{
	TRACE_AND_STOP;
	return -1;
}

int phy_ethtool_nway_reset(struct net_device *ndev)
{
	TRACE_AND_STOP;
	return -1;
}

int sysctl_tstamp_allow_data;
struct user_namespace init_user_ns;
