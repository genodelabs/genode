/*
 * \brief  USB net driver Linux emulation environment
 * \author Stefan Kalkowski
 * \date   2018-06-13
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SRC__DRIVERS__USB_NET__LX_EMUL_H_
#define _SRC__DRIVERS__USB_NET__LX_EMUL_H_

#include <base/fixed_stdint.h>
#include <stdarg.h>

#include <legacy/lx_emul/extern_c_begin.h>

#define __KERNEL__ 1

#include <legacy/lx_emul/compiler.h>
#include <legacy/lx_emul/printf.h>
#include <legacy/lx_emul/types.h>
#include <legacy/lx_emul/kernel.h>

#define __KERNEL_DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

enum { HZ = 100UL };

#include <legacy/lx_emul/jiffies.h>
#include <legacy/lx_emul/time.h>
#include <legacy/lx_emul/bitops.h>

typedef int clockid_t;

#include <legacy/lx_emul/timer.h>
#include <legacy/lx_emul/spinlock.h>
#include <legacy/lx_emul/mutex.h>

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u64 __be64;

#define __aligned_u64 __u64 __attribute__((aligned(8)))

#include <legacy/lx_emul/byteorder.h>
#include <legacy/lx_emul/atomic.h>
#include <legacy/lx_emul/work.h>
#include <legacy/lx_emul/bug.h>
#include <legacy/lx_emul/errno.h>
#include <legacy/lx_emul/module.h>
//#define __init
//#define __exit
//#define THIS_MODULE 0
//struct module;
//#define module_init(fn) int module_##fn(void) { return fn(); }
//#define module_exit(fn) void module_exit_##fn(void) { fn(); }
//#define EXPORT_SYMBOL_GPL(x)
//#define MODULE_AUTHOR(name)
//#define MODULE_DESCRIPTION(desc)
//#define MODULE_LICENSE(x)
//#define MODULE_VERSION(x)
//#define MODULE_PARM_DESC(_parm, desc)
//#define module_param(name, type, perm)
#include <legacy/lx_emul/gfp.h>
#include <legacy/lx_emul/barrier.h>

#define READ_ONCE(x) x

#include <legacy/lx_emul/list.h>
#include <legacy/lx_emul/string.h>
#include <legacy/lx_emul/kobject.h>
#include <legacy/lx_emul/completion.h>
#include <legacy/lx_emul/pm.h>
#include <legacy/lx_emul/scatterlist.h>

struct user_namespace {};

struct cred {
	struct user_namespace * user_ns;
};

struct file
{
	unsigned int f_flags;
	void * private_data;
	const struct cred * f_cred;
};

struct device;
struct device_driver;

typedef struct { __u8 b[16]; } uuid_le;
void * dev_get_drvdata(const struct device *dev);
int    dev_set_drvdata(struct device *dev, void *data);

#define netdev_dbg(dev, fmt, args...)
#define netdev_warn(dev, fmt, args...)  lx_printf("netdev_warn: " fmt, ##args)
#define netdev_err(dev, fmt, args...)   lx_printf("netdev_err:  " fmt, ##args)
#define netdev_info(dev, fmt, args...)  lx_printf("netdev_info: " fmt, ##args)

#define dev_info(dev, format, arg...) lx_printf("dev_info: " format , ## arg)
#define dev_warn(dev, format, arg...) lx_printf("dev_warn: " format , ## arg)
#define dev_err( dev, format, arg...) lx_printf("dev_err: "  format , ## arg)
#define dev_dbg( dev, format, arg...)

#define netif_info(priv, type, dev, fmt, args...) lx_printf("netif_info: " fmt, ## args);
#define netif_dbg(priv, type, dev, fmt, args...)
#define netif_err(priv, type, dev, fmt, args...)  lx_printf("netif_err: "  fmt, ## args);

#define pr_debug(fmt, ...)
#define pr_info(fmt, ...)       printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_err(fmt, ...)        printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_warn(fmt, ...)       printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_info_once(fmt, ...)  printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_notice(fmt, ...)     printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_emerg(fmt, ...)      printk(KERN_INFO fmt,   ##__VA_ARGS__)

#define try_then_request_module(x, mod...) (x)

struct bus_type
{
	int (*match)(struct device *dev, struct device_driver *drv);
	int (*probe)(struct device *dev);
};

struct device_driver
{
	char const      *name;
	struct bus_type *bus;
	struct module   *owner;
	const char      *mod_name;
};

typedef int devt;

 struct device_type {
	const char *name;
};

struct class
{
	const char *name;
	char *(*devnode)(struct device *dev, mode_t *mode);
};

struct device_node;

struct device
{
	char const               * name;
	struct device            * parent;
	struct kobject           * kobj;
	struct device_driver     * driver;
	struct bus_type          * bus;
	dev_t                      devt;
	struct class             * class;
	const struct device_type * type;
	void (*release)(struct device *dev);
	void                     * driver_data;
	struct device_node       * of_node;
};

#define module_driver(__driver, __register, __unregister, ...) \
	static int __init __driver##_init(void)                    \
	{                                                          \
		return __register(&(__driver) , ##__VA_ARGS__);        \
	}                                                          \
	module_init(__driver##_init);                              \
	static void __exit __driver##_exit(void)                   \
	{                                                          \
		__unregister(&(__driver) , ##__VA_ARGS__);             \
	}                                                          \
	module_exit(__driver##_exit);

#define KBUILD_MODNAME ""

void kfree(const void *);

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

void *kmalloc(size_t size, gfp_t flags);
void *kzalloc(size_t size, gfp_t flags);
int snprintf(char *buf, size_t size, const char *fmt, ...);
const char *dev_name(const struct device *dev);

struct __una_u16 { u16 x; } __attribute__((packed));
struct __una_u32 { u32 x; } __attribute__((packed));

#define get_unaligned(ptr) (*ptr)
u16 get_unaligned_le16(const void *p);
u32  get_unaligned_le32(const void *p);

struct completion
{
	unsigned int done;
	void * task;
};

struct notifier_block;

enum {
	ESHUTDOWN   = 58,
};

void msleep(unsigned int);

#define PAGE_SIZE 4096

#define rcu_assign_pointer(p,v) p = v

signed long schedule_timeout(signed long timeout);

int device_set_wakeup_enable(struct device *dev, bool enable);

struct tasklet_struct
{
	void (*func)(unsigned long);
	unsigned long data;
};

struct net_device;
struct ifreq;
struct sk_buff;
struct rtnl_link_stats64;

enum netdev_tx {
	NETDEV_TX_OK = 0x00,
	NETDEV_TX_BUSY = 0x10,
	NETDEV_TX_LOCKED = 0x20,
};
typedef enum netdev_tx netdev_tx_t;

#include <linux/netdev_features.h>

struct net_device_ops {
	int (*ndo_open) (struct net_device *dev);
	int (*ndo_stop) (struct net_device *dev);
	netdev_tx_t (*ndo_start_xmit) (struct sk_buff *skb, struct net_device *dev);
	void (*ndo_set_rx_mode) (struct net_device *dev);
	int (*ndo_change_mtu) (struct net_device *dev, int new_mtu);
	int (*ndo_validate_addr) (struct net_device *dev);
	void (*ndo_tx_timeout) (struct net_device *dev);
	int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
	int (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
	int (*ndo_set_features)(struct net_device *dev, netdev_features_t features);
	void (*ndo_get_stats64)(struct net_device *dev, struct rtnl_link_stats64 *storage);
};

struct net_device_stats
{
	unsigned long rx_packets;
	unsigned long tx_packets;
	unsigned long rx_bytes;
	unsigned long tx_bytes;
	unsigned long rx_errors;
	unsigned long tx_errors;
	unsigned long rx_dropped;
	unsigned long tx_dropped;
	unsigned long rx_length_errors;
	unsigned long rx_over_errors;
	unsigned long rx_crc_errors;
	unsigned long rx_frame_errors;
};

enum netdev_state_t {
	__LINK_STATE_START,
	__LINK_STATE_PRESENT,
	__LINK_STATE_NOCARRIER,
	__LINK_STATE_LINKWATCH_PENDING,
	__LINK_STATE_DORMANT,
};

enum { MAX_ADDR_LEN = 32, IFNAMESZ = 16 };

struct net_device
{
	char                         name[IFNAMESZ];
	unsigned long                state;
	netdev_features_t            features;
	struct net_device_stats      stats;
	netdev_features_t            hw_features;
	const struct net_device_ops *netdev_ops;
	const struct ethtool_ops    *ethtool_ops;
	const struct header_ops     *header_ops;
	unsigned int                 flags;
	unsigned int                 priv_flags;
	unsigned short               hard_header_len;
	unsigned char                min_header_len;
	unsigned long                mtu;
	unsigned long                min_mtu;
	unsigned long                max_mtu;
	unsigned short               type;
	unsigned char                addr_len;
	unsigned char               *dev_addr;
	unsigned char                broadcast[MAX_ADDR_LEN];
	unsigned long                tx_queue_len;
	int                          watchdog_timeo;
	struct timer_list            watchdog_timer;
	struct                       device dev;
	u16                          gso_max_segs;
	struct phy_device           *phydev;
	unsigned short               needed_headroom;
	unsigned short               needed_tailroom;
	void                        *priv;
	unsigned char                perm_addr[MAX_ADDR_LEN];
	unsigned char                addr_assign_type;
	int                          ifindex;
	void                        *session_component;
};

struct usbnet;

struct ethtool_eeprom;
struct ethtool_drvinfo;

struct sock;

struct kvec
{
	void  *iov_base;
	size_t iov_len;
};

struct iov_iter {};
size_t iov_iter_count(struct iov_iter *i);

typedef int raw_hdlc_proto;
typedef int cisco_proto;
typedef int fr_proto;
typedef int fr_proto_pvc;
typedef int fr_proto_pvc_info;
typedef int sync_serial_settings;
typedef int te1_settings;

struct rndis_indicate;

enum { ETH_ALEN = 6 };

int netif_running(const struct net_device *dev);
int phy_mii_ioctl(struct phy_device *phydev, struct ifreq *ifr, int cmd);
static inline void *netdev_priv(const struct net_device *dev) { return dev->priv; }

int usbnet_read_cmd(struct usbnet *dev, u8 cmd, u8 reqtype, u16 value, u16 index, void *data, u16 size);
int usbnet_read_cmd_nopm(struct usbnet *dev, u8 cmd, u8 reqtype, u16 value, u16 index, void *data, u16 size);

typedef __u32 __wsum;

static inline int rcu_read_lock_held(void) { return 1; }
static inline int rcu_read_lock_bh_held(void) { return 1; }

enum { NUMA_NO_NODE = -1 };

struct ts_state
{
	char cb[40];
};

struct ts_config
{
	unsigned int  (*get_next_block)(unsigned int consumed,
	                                const u8 **dst,
	                                struct ts_config *conf,
	                                struct ts_state *state);
	void          (*finish)(struct ts_config *conf,
	                        struct ts_state *state);
};

struct flow_dissector_key_control
{
	u16 thoff;
	u16 addr_type;
	u32 flags;
};

struct flow_keys
{
	struct flow_dissector_key_control control;
};

struct flow_dissector_key {};

struct flow_dissector {};

extern struct flow_dissector flow_keys_dissector;
extern struct flow_dissector flow_keys_buf_dissector;

struct flowi4 {};
struct flowi6 {};

__u32 __get_hash_from_flowi6(const struct flowi6 *fl6, struct flow_keys *keys);

bool flow_keys_have_l4(struct flow_keys *keys);

__u32 __get_hash_from_flowi4(const struct flowi4 *fl4, struct flow_keys *keys);

bool gfpflags_allow_blocking(const gfp_t gfp_flags);

struct lock_class_key { };

#define lockdep_set_class(lock, key)

struct page
{
	atomic_t _count;
	void *addr;
	dma_addr_t paddr;
	unsigned long private;
	unsigned long size;
} __attribute((packed));

static inline struct page *compound_head(struct page *page) { return page; }

bool page_is_pfmemalloc(struct page *page);

void __free_page_frag(void *addr);

struct page *alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order);

void get_page(struct page *page);
void put_page(struct page *page);

static inline void *page_address(struct page *page) { return page->addr; };

struct page_frag
{
	struct page *page;
	__u16        offset;
	__u16        size;
};

enum dma_data_direction { DMA_FROM_DEVICE = 2 };

dma_addr_t dma_map_page(struct device *dev, struct page *page, size_t offset, size_t size, enum dma_data_direction      dir);

void dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);

#define L1_CACHE_BYTES  32

size_t csum_and_copy_from_iter(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);

__wsum csum_block_add(__wsum csum, __wsum csum2, int offset);
__wsum csum_sub(__wsum csum, __wsum addend);
__wsum csum_partial(const void *buff, int len, __wsum sum);

bool csum_and_copy_from_iter_full(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);
bool copy_from_iter_full(void *addr, size_t bytes, struct iov_iter *i);
__wsum csum_block_sub(__wsum, __wsum, int);
typedef unsigned __poll_t;

typedef struct poll_table_struct { } poll_table;
size_t copy_to_iter(void *addr, size_t bytes, struct iov_iter *i);
struct timespec ktime_to_timespec(const ktime_t kt);

struct socket;
typedef u64 netdev_features_t;
typedef __u16 __sum16;

__sum16 csum_fold(__wsum csum);
__wsum csum_unfold(__sum16 n);
__wsum csum_add(__wsum csum, __wsum addend);
__wsum remcsum_adjust(void *ptr, __wsum csum, int start, int offset);

struct sk_buff;

void dev_kfree_skb_any(struct sk_buff *);


extern void page_frag_free(void *addr);

#define DECLARE_BITMAP(name,bits) unsigned long name[BITS_TO_LONGS(bits)]

struct rtnl_link_stats64;
struct ethtool_link_ksettings;
int netif_carrier_ok(const struct net_device *dev);

int is_valid_ether_addr(const u8 *);

void phy_print_status(struct phy_device *phydev);

enum {
	MII_BMCR      = 0x0,
	MII_BMSR      = 0x1,
	MII_PHYSID1   = 0x2,
	MII_PHYSID2   = 0x3,
	MII_ADVERTISE = 0x4,
	MII_LPA       = 0x5,
	MII_CTRL1000  = 0x9,
	MII_MMD_CTRL  = 0xd,
	MII_MMD_DATA  = 0xe,
	MII_PHYADDR   = 0x19,
	MII_MMD_CTRL_NOINCR = 0x4000,
};

enum { VLAN_HLEN = 4 };

void udelay(unsigned long usecs);

int eth_validate_addr(struct net_device *);

int netdev_mc_empty(struct net_device *);
unsigned netdev_mc_count(struct net_device * dev);

#define netdev_for_each_mc_addr(a, b) if (0)

void usleep_range(unsigned long min, unsigned long max);

void eth_hw_addr_random(struct net_device *dev);

static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
	*(u32 *)dst      = *(const u32 *)src;
	*(u16 *)(dst+ 4) = *(const u16 *)(src + 4);
}

u32 ether_crc(int, unsigned char *);

struct netdev_hw_addr
{
	unsigned char addr[MAX_ADDR_LEN];
};

void mdelay(unsigned long usecs);

int eth_mac_addr(struct net_device *, void *);

void netif_carrier_on(struct net_device *dev);
void netif_carrier_off(struct net_device *dev);

const void *of_get_mac_address(struct device_node *np);

u16 bitrev16(u16 in);
u16 crc16(u16 crc, const u8 *buffer, size_t len);
int hex2bin(u8 *dst, const char *src, size_t count);
char *hex_byte_pack(char *buf, u8 byte);

#define hex_asc_upper_lo(x)	hex_asc_upper[((x) & 0x0f)]
#define hex_asc_lo(x)	hex_asc[((x) & 0x0f)]
#define hex_asc_hi(x)	hex_asc[((x) & 0xf0) >> 4]


#define this_cpu_ptr(ptr) ptr

__be16 eth_type_trans(struct sk_buff *, struct net_device *);

struct u64_stats_sync {};

static inline unsigned long u64_stats_update_begin_irqsave(struct u64_stats_sync *syncp) {
	return 0; }

static inline void u64_stats_update_end_irqrestore(struct u64_stats_sync *syncp, unsigned long flags) { }

struct pcpu_sw_netstats
{
	u64                   rx_packets;
	u64                   rx_bytes;
	u64                   tx_packets;
	u64                   tx_bytes;
	struct u64_stats_sync syncp;
};

int netif_rx(struct sk_buff *);

enum { NET_RX_SUCCESS = 0 };
enum { SINGLE_DEPTH_NESTING = 1 };

void tasklet_schedule(struct tasklet_struct *t);
void tasklet_kill(struct tasklet_struct *t);

int  netif_device_present(struct net_device * d);
void netif_device_detach(struct net_device *dev);
void netif_stop_queue(struct net_device *);
void netif_start_queue(struct net_device *);
void netif_wake_queue(struct net_device * d);

void netdev_stats_to_stats64(struct rtnl_link_stats64 *stats64, const struct net_device_stats *netdev_stats);

enum { TASK_RUNNING = 0, TASK_INTERRUPTIBLE = 1, TASK_UNINTERRUPTIBLE = 2, TASK_NORMAL = 3 };

void __set_current_state(int state);
#define set_current_state(state) __set_current_state(state)

extern const struct cpumask *const cpu_possible_mask;

#define for_each_cpu(cpu, mask)                 \
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)

#define for_each_possible_cpu(cpu) for_each_cpu((cpu), cpu_possible_mask)

#define per_cpu_ptr(ptr, cpu)   ({ (void)(cpu);(typeof(*(ptr)) *)(ptr); })

u32 netif_msg_init(int, int);
#define netif_msg_tx_err(p) ({ printk("netif_msg_tx_err called not implemented\n"); 0; })
#define netif_msg_rx_err(p) ({ printk("netif_msg_rx_err called not implemented\n"); 0; })

unsigned int u64_stats_fetch_begin_irq(const struct u64_stats_sync *p);
bool u64_stats_fetch_retry_irq(const struct u64_stats_sync *p, unsigned int s);

void unregister_netdev(struct net_device *);

#define free_percpu(pdata) kfree(pdata)

void free_netdev(struct net_device *);

void netif_trans_update(struct net_device *dev);

void pm_runtime_enable(struct device *dev);

struct net_device *alloc_etherdev(int);

#define SET_NETDEV_DEV(net, pdev)        ((net)->dev.parent = (pdev))

void *__alloc_percpu(size_t size, size_t align);

#define alloc_percpu(type) \
	(typeof(type) __percpu *)__alloc_percpu(sizeof(type), __alignof__(type))


#define netdev_alloc_pcpu_stats(type) alloc_percpu(type)

enum {
	NETIF_MSG_DRV   = 0x1,
	NETIF_MSG_PROBE = 0x2,
	NETIF_MSG_LINK  = 0x4,
};

static inline bool ether_addr_equal(const u8 *addr1, const u8 *addr2)
{
	const u16 *a = (const u16 *)addr1;
	const u16 *b = (const u16 *)addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) == 0;
}

enum { NET_ADDR_RANDOM = 1 };

#define SET_NETDEV_DEVTYPE(net, devtype) ((net)->dev.type = (devtype))

int register_netdev(struct net_device *);

void netif_device_attach(struct net_device *dev);

enum { GFP_NOIO = GFP_LX_DMA };

void netif_tx_wake_all_queues(struct net_device *dev);

void eth_random_addr(u8 *addr);

long  __wait_completion(struct completion *work, unsigned long timeout);

struct mii_ioctl_data;

typedef int possible_net_t;

void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node);
bool gfp_pfmemalloc_allowed(gfp_t);

struct callback_head {
	struct callback_head *next;
	void (*func)(struct callback_head *head);
};
#define rcu_head callback_head

typedef int rwlock_t;

struct task_struct;

typedef struct { } read_descriptor_t;

#define SMP_CACHE_BYTES L1_CACHE_BYTES
#define ____cacheline_aligned        __attribute__((aligned(SMP_CACHE_BYTES)))
#define ____cacheline_aligned_in_smp __attribute__((aligned(SMP_CACHE_BYTES)))

struct percpu_counter {
	s64 count;
};

static inline int percpu_counter_init(struct percpu_counter *fbc, s64 amount, gfp_t gfp)
{
	fbc->count = amount;
	return 0;
}

static inline s64 percpu_counter_read(struct percpu_counter *fbc)
{
	return fbc->count;
}

static inline
void percpu_counter_add(struct percpu_counter *fbc, s64 amount)
{
	fbc->count += amount;
}

static inline
void __percpu_counter_add(struct percpu_counter *fbc, s64 amount, s32 batch)
{
	percpu_counter_add(fbc, amount);
}

s64 percpu_counter_sum_positive(struct percpu_counter *fbc);

static inline void percpu_counter_inc(struct percpu_counter *fbc)
{
	percpu_counter_add(fbc, 1);
}

static inline void percpu_counter_dec(struct percpu_counter *fbc)
{
	percpu_counter_add(fbc, -1);
}

static inline
s64 percpu_counter_read_positive(struct percpu_counter *fbc)
{
	return fbc->count;
}

void percpu_counter_destroy(struct percpu_counter *fbc);

s64 percpu_counter_sum(struct percpu_counter *fbc);

void bitmap_fill(unsigned long *dst, int nbits);
void bitmap_zero(unsigned long *dst, int nbits);

typedef unsigned seqlock_t;

enum { LL_MAX_HEADER  = 96 };

struct hh_cache
{
	u16         hh_len;
	u16       __pad;
	seqlock_t   hh_lock;

#define HH_DATA_MOD 16
#define HH_DATA_OFF(__len) \
	(HH_DATA_MOD - (((__len - 1) & (HH_DATA_MOD - 1)) + 1))
#define HH_DATA_ALIGN(__len) \
	(((__len)+(HH_DATA_MOD-1))&~(HH_DATA_MOD - 1))
	unsigned long   hh_data[HH_DATA_ALIGN(LL_MAX_HEADER) / sizeof(long)];
};

struct seq_net_private {
	struct net *net;
};

struct seq_file;
struct ctl_table;

typedef int proc_handler (struct ctl_table *ctl, int write, void __user *buffer, size_t *lenp, loff_t *ppos);

unsigned read_seqbegin(const seqlock_t *sl);
unsigned read_seqretry(const seqlock_t *sl, unsigned start);

int dev_queue_xmit(struct sk_buff *skb);

#define raw_smp_processor_id() 0

#define rcu_dereference_bh(p) p
#define rcu_dereference_raw(p) p
#define rcu_dereference_check(p, c) p
#define rcu_dereference(p) p

#define read_pnet(pnet) (&init_net)

static inline int net_eq(const struct net *net1, const struct net *net2) {
	return net1 == net2; }

extern struct net init_net;

struct net *dev_net(const struct net_device *dev);

#define __randomize_layout

struct cgroup;

#define mem_cgroup_sockets_enabled 0

struct mem_cgroup;

static inline bool mem_cgroup_under_socket_pressure(struct mem_cgroup *memcg) {
	return false; }

struct inode
{
//umode_t       i_mode;
	kuid_t        i_uid;
//unsigned long i_ino;
};

#define mutex_release(l, n, i)

int spin_is_locked(spinlock_t *lock);

void write_lock_bh(rwlock_t *);
void write_unlock_bh(rwlock_t *);

void security_sock_graft(struct sock *, struct socket *);

typedef unsigned  kgid_t;
kuid_t make_kuid(struct user_namespace *from, uid_t uid);

struct net
{
	struct user_namespace * user_ns;
};

u32 prandom_u32(void);
void rcu_read_lock(void);
void rcu_read_unlock(void);
#define rcu_dereference_protected(p, c) p
bool net_gso_ok(netdev_features_t features, int gso_type);
bool copy_from_iter_full_nocache(void *addr, size_t bytes, struct iov_iter *i);
bool lockdep_is_held(void *l);

extern int debug_locks;
bool wq_has_sleeper(struct wait_queue_head *wq_head);
bool poll_does_not_wait(const poll_table *p);
void poll_wait(struct file *f, wait_queue_head_t *w, poll_table *p);

struct task_struct
{
	unsigned int flags;
	struct page_frag task_frag;
};

extern struct task_struct *current;
int in_softirq(void);

enum { MAX_SCHEDULE_TIMEOUT = 1000 };

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

#define write_pnet(pnet, net) do { (void)(net);} while (0)

int l3mdev_master_ifindex_by_index(struct net *net, int ifindex);

struct kmem_cache;
void *kmem_cache_alloc_node(struct kmem_cache *cache, gfp_t, int);
void  kmem_cache_free(struct kmem_cache *, void *);
void  *kmem_cache_alloc(struct kmem_cache *, gfp_t);
struct page *virt_to_head_page(const void *x);

struct page_frag_cache
{
	bool pfmemalloc;
};

#define prefetchw(x) __builtin_prefetch(x,1)

size_t ksize(void *);

#define DEFINE_PER_CPU(type, name) \
	typeof(type) name

static inline unsigned long local_irq_save(unsigned long flags) { return flags; }
static inline void local_irq_restore(unsigned long f) { }

void *page_frag_alloc(struct page_frag_cache *nc, unsigned int fragsz, gfp_t gfp_mask);

enum {
	NAPI_STATE_SCHED,
	NAPI_STATE_DISABLE,
	NAPI_STATE_NPSVC,
	NAPI_STATE_HASHED,
};

struct napi_struct
{
	struct net_device * dev;
	int (*poll)(struct napi_struct *, int);
	unsigned long state;
	int weight;
};

void secpath_reset(struct sk_buff *);
int in_irq();

void trace_kfree_skb(struct sk_buff *, void *);
void trace_consume_skb(struct sk_buff *);

void kmem_cache_free_bulk(struct kmem_cache *, size_t, void **);

void dev_consume_skb_any(struct sk_buff *skb);

bool capable(int);

enum { PAGE_SHIFT = 12 };

unsigned long rlimit(unsigned int limit);

enum { RLIMIT_MEMLOCK = 8 };

struct user_struct
{
	atomic_long_t locked_vm;
};

struct user_struct *current_user();

static inline int atomic_long_cmpxchg(atomic_long_t *v, long old, long n) {
	return cmpxchg(&v->counter, old, n); }

struct user_struct *get_uid(struct user_struct *u);
void free_uid(struct user_struct *);

#define in_task() (1)

struct inet_skb_parm
{
	int iif;
};

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);
#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

#define page_private(page)      ((page)->private)
#define set_page_private(page, v)  ((page)->private = (v))

void *kmap_atomic(struct page *page);
void kunmap_atomic(void *addr);

#define CONFIG_LOCKDEP 1
#define CONFIG_NLS_DEFAULT "iso8859-1"

struct partial_page
{
	unsigned int offset;
	unsigned int len;
};

struct splice_pipe_desc
{
	struct page        **pages;
	struct partial_page *partial;
	int                  nr_pages;
	unsigned int         nr_pages_max;
	//unsigned int         flags;
	const struct pipe_buf_operations *ops;
	void (*spd_release)(struct splice_pipe_desc *, unsigned int);
};

struct page *virt_to_page(const void *x);

extern const struct pipe_buf_operations nosteal_pipe_buf_ops;

struct pipe_inode_info;
ssize_t splice_to_pipe(struct pipe_inode_info *, struct splice_pipe_desc *);

bool check_copy_size(const void *addr, size_t bytes, bool is_source);

__wsum csum_partial_ext(const void *buff, int len, __wsum sum);
__wsum csum_block_add_ext(__wsum csum, __wsum csum2, int offset, int len);
__wsum csum_partial_copy(const void *src, void *dst, int len, __wsum      sum);
#define csum_partial_copy_nocheck(src, dst, len, sum)   \
	csum_partial_copy((src), (dst), (len), (sum))

unsigned int textsearch_find(struct ts_config *, struct ts_state *);

__be16 skb_network_protocol(struct sk_buff *skb, int *depth);

bool can_checksum_protocol(netdev_features_t features, __be16 protocol);

unsigned int skb_gro_offset(const struct sk_buff *skb);

unsigned int skb_gro_len(const struct sk_buff *skb);

#define NAPI_GRO_CB(skb) ((struct napi_gro_cb *)(skb)->cb)

enum { NAPI_GRO_FREE = 1, NAPI_GRO_FREE_STOLEN_HEAD = 2, };

struct napi_gro_cb
{
	u16 flush;
	u16 count;
	u8  same_flow;
	u8 free;
	struct sk_buff *last;
};

enum {
	SLAB_HWCACHE_ALIGN = 0x00002000ul,
	SLAB_CACHE_DMA     = 0x00004000ul,
	SLAB_PANIC         = 0x00040000ul,
	SLAB_LX_DMA        = 0x80000000ul,
};

#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))

struct kmem_cache *kmem_cache_create_usercopy(const char *name, size_t size, size_t align, slab_flags_t flags, size_t useroffset, size_t usersize, void (*ctor)(void *));
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));

#define sg_is_last(sg)   ((sg)->page_link & 0x02)
void sg_mark_end(struct scatterlist *sg);
//void sg_set_buf(struct scatterlist *, const void *, unsigned int);
//void sg_set_page(struct scatterlist *, struct page *, unsigned int, unsigned int);

enum {
	IPPROTO_IP  = 0,
	IPPROTO_TCP = 6,
	IPPROTO_UDP = 17,
	IPPROTO_AH  = 51,
};

enum {
	IPPROTO_HOPOPTS  = 0,
	IPPROTO_ROUTING  = 43,
	IPPROTO_FRAGMENT = 44,
	IPPROTO_DSTOPTS  = 60,
};

void read_lock_bh(rwlock_t *);
void read_unlock_bh(rwlock_t *);

bool file_ns_capable(const struct file *file, struct user_namespace *ns, int cap);

extern struct user_namespace init_user_ns;

struct tcphdr
{
	__be16  source;
	__be16  dest;
	__be32  seq;
	__be32  ack_seq;
	__u16   res1:4,
	        doff:4,
	        fin:1,
	        syn:1,
	        rst:1,
	        psh:1,
	        ack:1,
	        urg:1,
	        ece:1,
	        cwr:1;
	__be16  window;
	__sum16 check;
};

struct tcphdr *tcp_hdr(const struct sk_buff *skb);

unsigned int tcp_hdrlen(const struct sk_buff *skb);

struct udphdr
{
	__sum16 check;
};

struct udphdr *udp_hdr(const struct sk_buff *skb);

struct iphdr {
	__u8 ihl:4;
	__u8 version:4;
	__u8    tos;
	__be16  tot_len;
	__be16  frag_off;
	__u8    ttl;
	__u8    protocol;
	__sum16 check;
	__be32  saddr;
	__be32  daddr;
};

struct iphdr *ip_hdr(const struct sk_buff *skb);

struct in6_addr {};

struct ipv6hdr
{
	__be16          payload_len;
	__u8            nexthdr;
	struct in6_addr saddr;
	struct in6_addr daddr;
};

struct ipv6hdr *ipv6_hdr(const struct sk_buff *skb);

struct ipv6_opt_hdr
{
	__u8 nexthdr;
	__u8 hdrlen;
} __attribute__((packed));

struct ip_auth_hdr
{
	__u8 nexthdr;
	__u8 hdrlen;
};

struct frag_hdr
{
	__u8    nexthdr;
	__be16  frag_off;
};

#define ipv6_optlen(p)  (((p)->hdrlen+1) << 3)
#define ipv6_authlen(p) (((p)->hdrlen+2) << 2)

enum { IP_OFFSET = 0x1FFF, IP_MF = 0x2000 };

enum { IP6_MF = 0x0001, IP6_OFFSET = 0xfff8 };

unsigned int ip_hdrlen(const struct sk_buff *skb);

__sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr, unsigned short len, unsigned short proto, __wsum sum);

__sum16 csum_ipv6_magic(const struct in6_addr *saddr, const struct in6_addr *daddr, __u32 len, unsigned short proto, __wsum csum);

void secpath_reset(struct sk_buff *);

struct tcphdr *inner_tcp_hdr(const struct sk_buff *skb);
unsigned int inner_tcp_hdrlen(const struct sk_buff *skb);

#define htons(x) __cpu_to_be16(x)
#define ntohs(x) __be16_to_cpu(x)

struct sctphdr
{
	unsigned unused;
};

enum {
	VLAN_CFI_MASK    = 0x1000,
	VLAN_TAG_PRESENT = VLAN_CFI_MASK
};

struct vlan_hdr
{
	__be16 h_vlan_TCI;
};

#define skb_vlan_tag_present(__skb)  ((__skb)->vlan_tci & VLAN_TAG_PRESENT)

void __vlan_hwaccel_put_tag(struct sk_buff *skb, __be16 vlan_proto, u16 vlan_tci);

void vlan_set_encap_proto(struct sk_buff *skb, struct vlan_hdr *vhdr);

enum { VLAN_ETH_HLEN = 18 };

static inline bool eth_type_vlan(__be16 ethertype) { return false; }

static inline int __vlan_insert_tag(struct sk_buff *skb, __be16 vlan_proto, u16 vlan_tci) {
	return 1; }

#define skb_vlan_tag_get(__skb)      ((__skb)->vlan_tci & ~VLAN_TAG_PRESENT)

extern struct workqueue_struct *tasklet_wq;

int __init netdev_boot_setup(char *str);

static inline void eth_zero_addr(u8 *addr) {
	memset(addr, 0x00, ETH_ALEN); }

#define FLOW_DISSECTOR_F_PARSE_1ST_FRAG     BIT(0)

static inline void eth_broadcast_addr(u8 *addr) {
	memset(addr, 0xff, ETH_ALEN); }

static inline bool is_multicast_ether_addr(const u8 *addr)
{
	return 0x01 & addr[0];
}

static inline bool is_multicast_ether_addr_64bits(const u8 addr[6+2])
{
	return is_multicast_ether_addr(addr);
}

static inline bool ether_addr_equal_64bits(const u8 addr1[6+2], const u8 addr2[6+2])
{
	const u16 *a = (const u16 *)addr1;
	const u16 *b = (const u16 *)addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) == 0;
}

bool netdev_uses_dsa(struct net_device *dev);

#include <uapi/linux/if_ether.h>

static inline bool eth_proto_is_802_3(__be16 proto)
{
	proto &= htons(0xFF00);
	return (u16)proto >= (u16)htons(ETH_P_802_3_MIN);
}

enum {
	IFF_LIVE_ADDR_CHANGE = 0x100000,
	IFF_TX_SKB_SHARING   = 0x10000,
};

enum { ARPHRD_ETHER = 1, };

struct neighbour;

struct header_ops
{
	int  (*create) (struct sk_buff *skb, struct net_device *dev,
	                unsigned short type, const void *daddr,
	                const void *saddr, unsigned int len);
	int  (*parse)(const struct sk_buff *skb, unsigned char *haddr);
	int  (*rebuild)(struct sk_buff *skb);
	int  (*cache)(const struct neighbour *neigh, struct hh_cache *hh, __be16 type);
	void (*cache_update)(struct hh_cache *hh,
	                     const struct net_device *dev,
	                     const unsigned char *haddr);
};

#define DEFAULT_TX_QUEUE_LEN 1000

struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name, unsigned char name_assign_type, void (*setup)(struct net_device *), unsigned int txqs, unsigned int rxqs);

enum { NET_NAME_UNKNOWN = 0 };

typedef void (*dr_release_t)(struct device *dev, void *res);
void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp);
void  devres_free(void *res);
void  devres_add(struct device *dev, void *res);

int scnprintf(char *buf, size_t size, const char *fmt, ...);

void *skb_gro_header_fast(struct sk_buff *skb, unsigned int offset);
void *skb_gro_header_hard(struct sk_buff *skb, unsigned int hlen);
void *skb_gro_header_slow(struct sk_buff *skb, unsigned int hlen, unsigned int offset);

static inline unsigned long compare_ether_header(const void *a, const void *b)
{
	u32 *a32 = (u32 *)((u8 *)a + 2);
	u32 *b32 = (u32 *)((u8 *)b + 2);

	return (*(u16 *)a ^ *(u16 *)b) | (a32[0] ^ b32[0]) |
		(a32[1] ^ b32[1]) | (a32[2] ^ b32[2]);

}

typedef struct sk_buff **(*gro_receive_t)(struct sk_buff **, struct sk_buff *);

struct offload_callbacks
{
	struct sk_buff **(*gro_receive)(struct sk_buff **head, struct sk_buff *skb);
	int              (*gro_complete)(struct sk_buff *skb, int nhoff);
};

struct packet_offload
{
	__be16                   type;
	u16                      priority;
	struct offload_callbacks callbacks;
};

struct packet_offload *gro_find_receive_by_type(__be16 type);

void skb_gro_pull(struct sk_buff *skb, unsigned int len);
void skb_gro_postpull_rcsum(struct sk_buff *skb, const void *start, unsigned int len);

struct sk_buff **call_gro_receive(gro_receive_t cb, struct sk_buff **head, struct sk_buff *skb);

void skb_gro_flush_final(struct sk_buff *skb, struct sk_buff **pp, int flush);

struct packet_offload *gro_find_complete_by_type(__be16 type);

void dev_add_offload(struct packet_offload *po);

#define fs_initcall(x)

#define __weak            __attribute__((weak))

unsigned char *arch_get_platform_mac_address(void);

#define to_pci_dev(n) NULL
struct pci_dev;
struct device_node * pci_device_to_OF_node(const struct pci_dev *pdev);

int dev_is_pci(struct device *dev);

void skb_init();
int module_usbnet_init();
int module_smsc95xx_driver_init();
int module_asix_driver_init();
int module_ax88179_178a_driver_init();
int module_cdc_driver_init();
int module_rndis_driver_init();

#include <uapi/linux/capability.h>
#include <uapi/linux/libc-compat.h>
#include <uapi/linux/if.h>
#include <uapi/linux/usb/cdc.h>
#include <uapi/linux/if_link.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/ethtool.h>
#include <uapi/linux/rtnetlink.h>
#include <uapi/linux/neighbour.h>
#include <uapi/linux/net_tstamp.h>
#include <linux/skbuff.h>
#include <linux/rculist.h>

#include <legacy/lx_emul/extern_c_end.h>

#endif /* _SRC__DRIVERS__USB_HID__LX_EMUL_H_ */
