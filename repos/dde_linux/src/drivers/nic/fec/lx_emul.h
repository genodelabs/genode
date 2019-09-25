/*
 * \brief  Freescale ethernet driver Linux emulation environment
 * \author Stefan Kalkowski
 * \date   2017-11-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SRC__DRIVERS__NIC__FEC__LX_EMUL_H_
#define _SRC__DRIVERS__NIC__FEC__LX_EMUL_H_

#include <base/fixed_stdint.h>
#include <stdarg.h>

#include <lx_emul/extern_c_begin.h>

#include <lx_emul/barrier.h>
#include <lx_emul/compiler.h>
#include <lx_emul/printf.h>
#include <lx_emul/types.h>

static inline void __read_once_size(const volatile void *p, void *res, int size)
{
	switch (size) {
		case 1: *(__u8  *)res = *(volatile __u8  *)p; break;
		case 2: *(__u16 *)res = *(volatile __u16 *)p; break;
		case 4: *(__u32 *)res = *(volatile __u32 *)p; break;
		case 8: *(__u64 *)res = *(volatile __u64 *)p; break;
		default:
			barrier();
			__builtin_memcpy((void *)res, (const void *)p, size);
			barrier();
	}
}

#ifdef __cplusplus
#define READ_ONCE(x) \
({ \
	barrier(); \
	x; \
})
#else
#define READ_ONCE(x) \
({                                               \
	union { typeof(x) __val; char __c[1]; } __u; \
	__read_once_size(&(x), __u.__c, sizeof(x));  \
	__u.__val;                                   \
})
#endif


#include <lx_emul/list.h>

void lx_backtrace(void);

#define DEBUG_LINUX_PRINTK 0

#define DEBUG 0
#if DEBUG
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
		lx_backtrace(); \
		BUG(); \
	} while (0)

#define ASSERT(x) \
	do { \
		if (!(x)) { \
			lx_printf("%s:%u assertion failed\n", __func__, __LINE__); \
			BUG(); \
		} \
	} while (0)

typedef int clockid_t;

#define PAGE_SIZE 4096UL

enum { PAGE_SHIFT = 12 };
enum { HZ = 100UL, };

typedef __u16 __le16;
typedef __u32 __le32;
typedef __u64 __le64;
typedef __u16 __be16;
typedef __u32 __be32;
typedef __u64 __be64;
typedef __s64 time64_t;

#define L1_CACHE_BYTES  32
#define SMP_CACHE_BYTES L1_CACHE_BYTES

#define __aligned_u64 __u64 __attribute__((aligned(8)))
#define ____cacheline_aligned_in_smp __attribute__((aligned(SMP_CACHE_BYTES)))
#define ____cacheline_aligned        __attribute__((aligned(SMP_CACHE_BYTES)))


struct iov_iter { };
size_t iov_iter_count(struct iov_iter *i);

#define dev_info(  dev, format, arg...) lx_printf("dev_info: "   format , ## arg)
#define dev_warn(  dev, format, arg...) lx_printf("dev_warn: "   format , ## arg)
#define dev_WARN(  dev, format, arg...) lx_printf("dev_WARN: "   format , ## arg)
#define dev_err(   dev, format, arg...) lx_printf("dev_error: "  format , ## arg)
#define dev_notice(dev, format, arg...) lx_printf("dev_notice: " format , ## arg)
#define dev_crit(  dev, format, arg...) lx_printf("dev_crit: "   format , ## arg)
#if DEBUG
#define dev_dbg(   dev, format, arg...) printk("dev_dbg: "    format , ## arg)
#else
#define dev_dbg(   dev, format, arg...)
#endif

#define pr_debug(fmt, ...)      printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_info(fmt, ...)       printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_err(fmt, ...)        printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_warn(fmt, ...)       printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_info_once(fmt, ...)  printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_notice(fmt, ...)     printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_emerg(fmt, ...)      printk(KERN_INFO fmt,   ##__VA_ARGS__)

#define netdev_err(dev, fmt, args...)  lx_printf("netdev_err:  " fmt, ##args)
#define netdev_warn(dev, fmt, args...) lx_printf("netdev_warn: " fmt, ##args)
#define netdev_info(dev, fmt, args...) lx_printf("netdev_info: " fmt, ##args)

#include <lx_emul/kernel.h>
#include <lx_emul/irq.h>
#include <lx_emul/jiffies.h>
#include <lx_emul/time.h>
#include <lx_emul/timer.h>

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

#include <lx_emul/mutex.h>

LX_MUTEX_INIT_DECLARE(mdio_board_lock);
LX_MUTEX_INIT_DECLARE(phy_fixup_lock);

#define mdio_board_lock LX_MUTEX(mdio_board_lock)
#define phy_fixup_lock LX_MUTEX(phy_fixup_lock)

#include <lx_emul/bitops.h>
#include <lx_emul/atomic.h>
#include <lx_emul/work.h>
#include <lx_emul/spinlock.h>
#include <lx_emul/errno.h>
#include <lx_emul/string.h>
#include <lx_emul/module.h>
#include <lx_emul/bug.h>
#include <lx_emul/gfp.h>

enum {
	__GFP_COLD   = 0x00000100u,
	__GFP_REPEAT = 0x00000400u,
};

#include <uapi/linux/swab.h>
#include <lx_emul/byteorder.h>
#include <lx_emul/completion.h>
#include <lx_emul/ioport.h>
#include <uapi/linux/net_tstamp.h>
#include <uapi/linux/ptp_clock.h>
#include <lx_emul/pm.h>
#include <lx_emul/scatterlist.h>
#include <lx_emul/kobject.h>

enum {
	ETH_HLEN      = 14,
	ETH_ALEN      = 6,      /* octets in one ethernet addr */
	ETH_P_8021Q   = 0x8100, /* 802.1Q VLAN Extended Header  */
	ETH_P_IP      = 0x0800,
	ETH_P_IPV6    = 0x86DD,
	ETH_P_8021AD  = 0x88A8,
	VLAN_HLEN     = 4,
	VLAN_ETH_HLEN = 18,
};

typedef int raw_hdlc_proto;
typedef int cisco_proto;
typedef int fr_proto;
typedef int fr_proto_pvc;
typedef int fr_proto_pvc_info;
typedef int sync_serial_settings;
typedef int te1_settings;

struct callback_head {
	struct callback_head *next;
	void (*func)(struct callback_head *head);
};
#define rcu_head callback_head

#define FIONREAD 0x541B
#define TIOCOUTQ 0x5411

struct completion { unsigned int done; void *task; };
long  __wait_completion(struct completion *work, unsigned long timeout);

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

#define writel(value, addr) (*(volatile uint32_t *)(addr) = (value))
#define readl(addr)         (*(volatile uint32_t *)(addr))

typedef u64 cycle_t;

struct timespec64 {
	time64_t tv_sec;  /* seconds */
	long     tv_nsec; /* nanoseconds */
};

extern struct timespec64 ns_to_timespec64(const s64 nsec);

static inline s64 timespec64_to_ns(const struct timespec64 *ts)
{
	return ((s64) ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

#define ktime_to_ns(kt) ((kt))

ktime_t ns_to_ktime(u64 ns);

struct device;
struct device_driver;

struct bus_type
{
	const char * name;
	const struct attribute_group **dev_groups;

	int (*match)(struct device *dev, struct device_driver *drv);
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	int (*probe)(struct device *dev);

	const struct dev_pm_ops *pm;
};

struct device_driver {
	const char * name;
	struct bus_type *bus;
	struct module   *owner;
	const struct of_device_id*of_match_table;
	const struct dev_pm_ops *pm;
	int (*probe)(struct device *dev);
	int (*remove) (struct device *dev);
};

struct class
{
	const char *name;
	void (*dev_release)(struct device *dev);
};

struct attribute {
	const char *name;
};

struct attribute_group
{
	struct attribute ** attrs;
};

struct platform_device;

struct device {
	char name[32];
	struct device * parent;
	struct kobject kobj;
	struct device_driver *driver;
	void * platform_data;
	void * driver_data;
	const struct attribute_group **groups;
	void (*release)(struct device *dev);
	struct bus_type *bus;
	struct class *class;
	struct device_node *of_node;
	struct fwnode_handle *fwnode;
	struct platform_device *plat_dev;
};

struct platform_device {
	const char * name;
	struct device dev;
	const struct platform_device_id * id_entry;
};

#define platform_get_device_id(pdev) ((pdev)->id_entry)

static inline void *platform_get_drvdata(const struct platform_device *pdev)
{
	return pdev->dev.driver_data;
}

void udelay(unsigned long usecs);

enum netdev_tx {
	NETDEV_TX_OK = 0x00,
	NETDEV_TX_BUSY = 0x10,
	NETDEV_TX_LOCKED = 0x20,
};
typedef enum netdev_tx netdev_tx_t;

struct sk_buff;
struct ifreq;

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
};

struct net_device_stats {
	unsigned long rx_packets;
	unsigned long tx_packets;
	unsigned long rx_bytes;
	unsigned long tx_bytes;
	unsigned long collisions;
	unsigned long rx_errors;
	unsigned long tx_errors;
	unsigned long rx_dropped;
	unsigned long tx_dropped;
	unsigned long rx_length_errors;
	unsigned long rx_over_errors;
	unsigned long rx_crc_errors;
	unsigned long rx_frame_errors;
	unsigned long rx_fifo_errors;
	unsigned long rx_missed_errors;
	unsigned long tx_aborted_errors;
	unsigned long tx_carrier_errors;
	unsigned long tx_fifo_errors;
	unsigned long tx_heartbeat_errors;
	unsigned long tx_window_errors;
};

struct netdev_hw_addr {
	struct list_head list;
	unsigned char addr[32];
};

struct netdev_hw_addr_list {
	struct list_head list;
	int count;
};

enum { NETDEV_ALIGN = 32, GSO_MAX_SEGS = 65535 };

enum netdev_state_t {
	__LINK_STATE_START,
	__LINK_STATE_PRESENT,
	__LINK_STATE_NOCARRIER,
	__LINK_STATE_LINKWATCH_PENDING,
	__LINK_STATE_DORMANT,
};

#define MAX_ADDR_LEN    32

struct net_device
{
	const char *                 name;
	unsigned long                state;
	netdev_features_t            features;
	struct net_device_stats      stats;
	netdev_features_t            hw_features;
	int                          ifindex;
	const struct net_device_ops *netdev_ops;
	const struct ethtool_ops    *ethtool_ops;
	const struct header_ops     *header_ops;
	unsigned int                 flags;
	unsigned int                 priv_flags;
	unsigned short               hard_header_len;
	unsigned long                mtu;
	unsigned int                 min_mtu;
	unsigned long                max_mtu;
	unsigned short               type;
	unsigned char                min_header_len;
	unsigned char                addr_len;
	struct netdev_hw_addr_list   mc;
	unsigned char               *dev_addr;
	unsigned char                broadcast[MAX_ADDR_LEN];
	unsigned long                tx_queue_len;
	int                          watchdog_timeo;
	struct timer_list            watchdog_timer;
	struct device                dev;
	u16                          gso_max_segs;
	struct phy_device           *phydev;
};

static inline void *netdev_priv(const struct net_device *dev) {
	return (char *)dev + ALIGN(sizeof(struct net_device), NETDEV_ALIGN); }

#define CLOCKSOURCE_MASK(bits) (cycle_t)((bits) < 64 ? ((1ULL<<(bits))-1) : -1)

static inline u64 div_u64(u64 dividend, u32 divisor) { return dividend / divisor; }

struct pps_event_time {
	struct timespec64 ts_real;
};

size_t copy_from_user(void *to, void const *from, size_t len);
size_t copy_to_user(void *dst, void const *src, size_t len);

int snprintf(char *buf, size_t size, const char *fmt, ...);

struct clk
{
	const char * name;
	unsigned long rate;
};

unsigned long clk_get_rate(struct clk * clk);

#define module_param_array(macaddr, byte, arg1, arg2);

# define swab32p __swab32p
# define swab32s __swab32s

u64 local_clock(void);

#define do_div(n,base) ({ \
	unsigned long __base = (base); \
	unsigned long __rem; \
	__rem = ((uint64_t)(n)) % __base; \
	(n)   = ((uint64_t)(n)) / __base; \
	__rem; \
})

enum {
	MSEC_PER_SEC  = 1000L,
	USEC_PER_SEC  = MSEC_PER_SEC * 1000L,
};

static inline int rcu_read_lock_held(void) { return 1; }
static inline int rcu_read_lock_bh_held(void) { return 1; }

unsigned int jiffies_to_usecs(const unsigned long j);

#define __aligned(x) __attribute__((aligned(x)))

#define kmemcheck_bitfield_begin(name)
#define kmemcheck_bitfield_end(name)

typedef __u32 __wsum;

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

dma_addr_t dma_map_page(struct device *dev, struct page *page, size_t offset, size_t size, enum dma_data_direction dir);

void dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);

#define L1_CACHE_BYTES  32


size_t csum_and_copy_from_iter(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);

__wsum csum_block_add(__wsum csum, __wsum csum2, int offset);
__wsum csum_sub(__wsum csum, __wsum addend);
__wsum csum_partial(const void *buff, int len, __wsum sum);

typedef struct poll_table_struct { } poll_table;

size_t copy_from_iter(void *addr, size_t bytes, struct iov_iter *i);
size_t copy_to_iter(void *addr, size_t bytes, struct iov_iter *i);

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
	unsigned int         flags;
	const struct pipe_buf_operations *ops;
	void (*spd_release)(struct splice_pipe_desc *, unsigned int);
};

struct timespec ktime_to_timespec(const ktime_t kt);

typedef __u16 __sum16;

__sum16 csum_fold(__wsum csum);
__wsum csum_add(__wsum csum, __wsum addend);
__wsum remcsum_adjust(void *ptr, __wsum csum, int start, int offset);

#define htons(x) __cpu_to_be16(x)

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

struct sk_buff;
struct iphdr *ip_hdr(const struct sk_buff *skb);

typedef unsigned short ushort;

dma_addr_t dma_map_single(struct device *dev, void *ptr, size_t size, enum dma_data_direction dir);

enum { DMA_TO_DEVICE = 1 };

int dma_mapping_error(struct device *dev, dma_addr_t dma_addr);

void dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);

void dev_kfree_skb_any(struct sk_buff *);

int net_ratelimit(void);

unsigned int tcp_hdrlen(const struct sk_buff *skb);

struct netdev_queue *netdev_get_tx_queue(const struct net_device *dev, unsigned int index);
void netif_tx_stop_queue(struct netdev_queue *dev_queue);
void netif_tx_wake_queue(struct netdev_queue *dev_queue);
bool netif_queue_stopped(const struct net_device *dev);

#define CONFIG_ARM      1
#define CONFIG_ARCH_MXC 1
#define CONFIG_DEBUG_LOCK_ALLOC 1
#define CONFIG_MDIO_DEVICE 1
#define CONFIG_OF_MDIO  1
#define CONFIG_PHYLIB 1
#define CONFIG_PTP_1588_CLOCK 1

void rtnl_lock(void);
void rtnl_unlock(void);

int netif_device_present(struct net_device *);
int netif_running(const struct net_device *dev);
void netif_wake_queue(struct net_device *);
void netif_tx_lock_bh(struct net_device *dev);
void netif_tx_unlock_bh(struct net_device *dev);

void napi_enable(struct napi_struct *n);
void napi_disable(struct napi_struct *n);

#define __randomize_layout

extern unsigned long find_next_bit(const unsigned long *addr, unsigned long
                                   size, unsigned long offset);
#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)

#define prefetch(x)  __builtin_prefetch(x)
#define prefetchw(x) __builtin_prefetch(x,1)

#define ntohs(x) __be16_to_cpu(x)

struct vlan_hdr
{
	__be16 h_vlan_TCI;
};

__be16 eth_type_trans(struct sk_buff *, struct net_device *);

void __vlan_hwaccel_put_tag(struct sk_buff *skb, __be16 vlan_proto, u16 vlan_tci);

enum gro_result {
	GRO_MERGED,
	GRO_MERGED_FREE,
	GRO_HELD,
	GRO_NORMAL,
	GRO_DROP,
};
typedef enum gro_result gro_result_t;

gro_result_t napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb);

void dma_sync_single_for_device(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);

void *dev_get_platdata(const struct device *dev);

int is_valid_ether_addr(const u8 *);

const void *of_get_mac_address(struct device_node *np);

void eth_hw_addr_random(struct net_device *dev);

int pm_runtime_get_sync(struct device *dev);

void reinit_completion(struct completion *x);

void pm_runtime_mark_last_busy(struct device *dev);

int pm_runtime_put_autosuspend(struct device *dev);

int clk_prepare_enable(struct clk *);

void clk_disable_unprepare(struct clk *);

struct phy_device *of_phy_connect(struct net_device *dev, struct device_node *phy_np, void (*hndlr)(struct net_device *), u32 flags, int iface);

const char *dev_name(const struct device *dev);

void *kmalloc(size_t size, gfp_t flags);
void kfree(const void *);
void *kzalloc(size_t size, gfp_t flags);

struct mii_bus;
struct device_node *of_get_child_by_name( const struct device_node *node, const char *name);
void of_node_put(struct device_node *node);
int of_mdiobus_register(struct mii_bus *mdio, struct device_node *np);

struct resource *platform_get_resource(struct platform_device *, unsigned, unsigned);

/*******************************
 ** asm-generic/atomic-long.h **
 *******************************/

static inline int atomic_long_cmpxchg(atomic_long_t *v, long old, long n) {
	return cmpxchg(&v->counter, old, n); }

/************************
 ** linux/capability.h **
 ************************/

bool capable(int);

/*************************
 ** linux/cgroup-defs.h **
 *************************/

struct cgroup;

/******************
 ** linux/cred.h **
 ******************/

struct user_struct *current_user();

/*******************
 ** linux/delay.h **
 *******************/

void usleep_range(unsigned long min, unsigned long max);

/*************************
 ** linux/etherdevice.h **
 *************************/

static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
	*(u32 *)dst      = *(const u32 *)src;
	*(u16 *)(dst+ 4) = *(const u16 *)(src + 4);
}

/*********************
 ** linux/ethtool.h **
 *********************/

struct ethtool_link_ksettings;

/****************
 ** linux/fs.h **
 ****************/

typedef struct {
	size_t written;
	size_t count;
	union {
		char __user *buf;
		void *data;
	} arg;
	int error;
} read_descriptor_t;

/********************
 ** linux/fwnode.h **
 ********************/

struct fwnode_handle { int dummy; };

/**********************
 ** linux/mm_types.h **
 **********************/

struct page_frag_cache
{
	bool pfmemalloc;
};

/*****************
 ** linux/gfp.h **
 *****************/

void *page_frag_alloc(struct page_frag_cache *nc,
                      unsigned int fragsz, gfp_t gfp_mask);

void page_frag_free(void *addr);

/*********************
 ** linux/if_vlan.h **
 *********************/

static inline bool eth_type_vlan(__be16 ethertype) { return false; }

/***********************
 ** linux/interrupt.h **
 ***********************/

#define IRQF_SHARED		0x00000080
#define IRQF_ONESHOT		0x00002000

int request_threaded_irq(unsigned int irq, irq_handler_t handler,
                         irq_handler_t thread_fn,
                         unsigned long flags, const char *name, void *dev);

/*********************
 ** linux/lockdep.h **
 *********************/

struct lockdep_map { };

#define mutex_release(l, n, i)

/*****************************
 ** linux/mod_devicetable.h **
 *****************************/

#define MDIO_NAME_SIZE		32

/************************
 ** linux/memcontrol.h **
 ************************/

struct mem_cgroup;

#define mem_cgroup_sockets_enabled 0

static inline bool mem_cgroup_under_socket_pressure(struct mem_cgroup *memcg) {
	return false; }

/***********************
 ** linux/netdevice.h **
 ***********************/

void __napi_schedule(struct napi_struct *n);

typedef struct sk_buff **(*gro_receive_t)(struct sk_buff **, struct sk_buff *);

struct sk_buff **call_gro_receive(gro_receive_t cb, struct sk_buff **head, struct sk_buff *skb);

void dev_consume_skb_any(struct sk_buff *skb);

bool napi_complete_done(struct napi_struct *n, int work_done);

bool napi_schedule_prep(struct napi_struct *n);

void skb_gro_flush_final(struct sk_buff *skb, struct sk_buff **pp, int flush);

/****************
 ** linux/of.h **
 ****************/

struct device_node
{
	const char * full_name;
	struct fwnode_handle fwnode;
};

#define of_fwnode_handle(node)						\
	({								\
		typeof(node) __of_fwnode_handle_node = (node);		\
									\
		__of_fwnode_handle_node ?				\
			&__of_fwnode_handle_node->fwnode : NULL;	\
	})

int of_machine_is_compatible(const char *compat);

bool of_property_read_bool(const struct device_node *np, const char *propname);

/***********************
 ** linux/of_device.h **
 ***********************/

int of_device_uevent_modalias(struct device *dev, struct kobj_uevent_env *env);

/*********************
 ** linux/of_mdio.h **
 *********************/

int of_mdio_parse_addr(struct device *dev, const struct device_node *np);

void of_phy_deregister_fixed_link(struct device_node *np);

/*****************
 ** linux/pci.h **
 *****************/

int dev_is_pci(struct device *dev);

struct pci_dev;

struct device_node * pci_device_to_OF_node(const struct pci_dev *pdev);

#define to_pci_dev(n) NULL

/******************************
 ** linux/phy_led_triggers.h **
 ******************************/

void phy_led_trigger_change_speed(struct phy_device *phy);

int phy_led_triggers_register(struct phy_device *phy);

void phy_led_triggers_unregister(struct phy_device *phy);

/*****************************
 ** linux/platform_device.h **
 *****************************/

int platform_get_irq_byname(struct platform_device *dev, const char *name);

int platform_irq_count(struct platform_device *);

/************************
 ** linux/pm_runtime.h **
 ************************/

void pm_runtime_disable(struct device *dev);

int pm_runtime_put(struct device *dev);

/*********************
 ** linux/preempt.h **
 *********************/

#define in_task() (1)

/**********************
 ** linux/rcupdate.h **
 **********************/

#define rcu_assign_pointer(p, v) (p = v);

#define rcu_dereference_protected(p, c) p

/*************************
 ** linux/scatterlist.h **
 *************************/

#define sg_is_last(sg)   ((sg)->page_link & 0x02)

/**************************
 ** linux/sched/signal.h **
 **************************/

unsigned long rlimit(unsigned int limit);

/************************
 ** linux/sched/user.h **
 ************************/

struct user_struct
{
	atomic_long_t locked_vm;
};

void free_uid(struct user_struct *);

struct user_struct *get_uid(struct user_struct *u);

/******************
 ** linux/sctp.h **
 ******************/

struct sctphdr
{
	unsigned unused;
};

struct kmem_cache *kmem_cache_create_usercopy(const char *name, size_t size,
                                              size_t align, slab_flags_t flags,
                                              size_t useroffset, size_t usersize,
                                              void (*ctor)(void *));

/******************
 ** linux/slab.h **
 ******************/

void *kcalloc(size_t n, size_t size, gfp_t flags);

void kmem_cache_free_bulk(struct kmem_cache *, size_t, void **);

/**********************
 ** linux/spinlock.h **
 **********************/

int spin_is_locked(spinlock_t *lock);

/********************
 ** linux/stddef.h **
 ********************/

#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))

/*******************
 ** linux/sysfs.h **
 *******************/

int sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name);

int sysfs_create_link_nowarn(struct kobject *kobj, struct kobject *target, const char *name);

void sysfs_remove_link(struct kobject *kobj, const char *name);

/*************************
 ** linux/thread_info.h **
 *************************/

static inline void check_object_size(const void *ptr, unsigned long n,
				                     bool to_user) { }

void __bad_copy_from(void);
void __bad_copy_to(void);

static inline void copy_overflow(int size, unsigned long count)
{
	WARN(1, "Buffer overflow detected (%d < %lu)!\n", size, count);
}

static __always_inline bool
check_copy_size(const void *addr, size_t bytes, bool is_source)
{
	int sz = __compiletime_object_size(addr);
	if (unlikely(sz >= 0 && sz < bytes)) {
		if (!__builtin_constant_p(bytes))
			copy_overflow(sz, bytes);
		else if (is_source)
			__bad_copy_from();
		else
			__bad_copy_to();
		return false;
	}
	check_object_size(addr, bytes, is_source);
	return true;
}

/****************************
 ** linux/user_namespace.h **
 ****************************/

struct user_namespace { };

/********************
 ** linux/uidgid.h **
 ********************/

kuid_t make_kuid(struct user_namespace *from, uid_t uid);

/*****************
 ** linux/uio.h **
 *****************/

struct kvec
{
	void  *iov_base;
	size_t iov_len;
};

bool _copy_from_iter_full(void *addr, size_t bytes, struct iov_iter *i);

static __always_inline __must_check
bool copy_from_iter_full(void *addr, size_t bytes, struct iov_iter *i)
{
	if (unlikely(!check_copy_size(addr, bytes, false)))
		return false;
	else
		return _copy_from_iter_full(addr, bytes, i);
}

bool copy_from_iter_full_nocache(void *addr, size_t bytes, struct iov_iter *i);

bool csum_and_copy_from_iter_full(void *addr, size_t bytes, __wsum *csum, struct iov_iter *i);

/******************
 ** linux/wait.h **
 ******************/

bool wq_has_sleeper(struct wait_queue_head *wq_head);

/********************
 ** net/checksum.h **
 ********************/

static inline __wsum
csum_block_sub(__wsum csum, __wsum csum2, int offset)
{
	return csum_block_add(csum, ~csum2, offset);
}

static inline __wsum csum_unfold(__sum16 n)
{
	return (__force __wsum)n;
}

/**************************
 ** net/flow_dissector.h **
 **************************/

#define FLOW_DISSECTOR_F_PARSE_1ST_FRAG		BIT(0)

/*************************
 ** net/net_namespace.h **
 *************************/

struct net;

/******************
 ** net/l3mdev.h **
 ******************/

int l3mdev_master_ifindex_by_index(struct net *net, int ifindex);

/*********************
 ** net/pkt_sched.h **
 *********************/

#define DEFAULT_TX_QUEUE_LEN	1000

/***********************
 ** soc/imx/cpuidle.h **
 ***********************/

static inline void imx6q_cpuidle_fec_irqs_used(void) { }
static inline void imx6q_cpuidle_fec_irqs_unused(void) { }

/*************************
 ** trace/events/mdio.h **
 *************************/

void trace_mdio_access(void *dummy, ...);

/*********************************
 ** uapi/asm-generic/resource.h **
 *********************************/

# define RLIMIT_MEMLOCK		8	/* max locked-in-memory address space */

/*****************************
 ** uapi/linux/capability.h **
 *****************************/

#define CAP_IPC_LOCK         14

/*************************
 ** uapi/linux/kernel.h **
 *************************/

#define __KERNEL_DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/******************************
 ** uapi/linux/libc-compat.h **
 ******************************/

#define __UAPI_DEF_IF_IFMAP 1
#define __UAPI_DEF_IF_IFNAMSIZ 1
#define __UAPI_DEF_IF_IFREQ 1
#define __UAPI_DEF_IF_NET_DEVICE_FLAGS 1

/************************
 ** uapi/linux/types.h **
 ************************/
 
typedef unsigned __poll_t;

/************************/

#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]

#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/ethtool.h>
#include <uapi/linux/if.h>
#include <uapi/linux/mdio.h>
#include <uapi/linux/mii.h>
#include <uapi/linux/sockios.h>

int ethtool_op_get_ts_info(struct net_device *, struct ethtool_ts_info *);

int  device_set_wakeup_enable(struct device *dev, bool enable);

bool device_may_wakeup(struct device *dev);

int enable_irq_wake(unsigned int irq);
int disable_irq_wake(unsigned int irq);

u32 ethtool_op_get_link(struct net_device *);

void *dma_alloc_coherent(struct device *, size_t, dma_addr_t *, gfp_t);
void dma_free_coherent(struct device *, size_t size, void *vaddr, dma_addr_t bus);

void netif_tx_start_all_queues(struct net_device *dev);

int pinctrl_pm_select_default_state(struct device *dev);

int pinctrl_pm_select_sleep_state(struct device *dev);

void netif_tx_disable(struct net_device *dev);

#include <linux/rculist.h>

#define netdev_hw_addr_list_for_each(ha, l) \
	list_for_each_entry(ha, &(l)->list, list)

#define netdev_for_each_mc_addr(ha, dev) \
	netdev_hw_addr_list_for_each(ha, &(dev)->mc)

void netif_tx_wake_all_queues(struct net_device *);

int eth_validate_addr(struct net_device *);

void *dmam_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp);

int eth_change_mtu(struct net_device *dev, int new_mtu);
int eth_validate_addr(struct net_device *dev);

void netif_napi_add(struct net_device *dev, struct napi_struct *napi, int (*poll)(struct napi_struct *, int), int weight);

bool of_device_is_available(const struct device_node *device);
int of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value);

enum { NAPI_POLL_WEIGHT = 64 };

#define SET_NETDEV_DEV(net, pdev)        ((net)->dev.parent = (pdev))

struct net_device *alloc_etherdev_mqs(int sizeof_priv, unsigned int txqs, unsigned int rxqs);

void *devm_ioremap_resource(struct device *dev, struct resource *res);

const struct of_device_id *of_match_device(const struct of_device_id *matches, const struct device *dev);

const void *of_get_property(const struct device_node *node, const char *name, int *lenp);

void platform_set_drvdata(struct platform_device *pdev, void *data);

struct device_node *of_parse_phandle(const struct device_node *np, const char *phandle_name, int index);

int of_phy_register_fixed_link(struct device_node *np);
bool of_phy_is_fixed_link(struct device_node *np);
struct device_node *of_node_get(struct device_node *node);
int of_get_phy_mode(struct device_node *np);

struct clk *devm_clk_get(struct device *dev, const char *id);
struct regulator *__must_check devm_regulator_get(struct device *dev, const char *id);

void pm_runtime_set_autosuspend_delay(struct device *dev, int delay);
void pm_runtime_use_autosuspend(struct device *dev);
void pm_runtime_get_noresume(struct device *dev);
int  pm_runtime_set_active(struct device *dev);
void pm_runtime_enable(struct device *dev);

int regulator_enable(struct regulator *);

int platform_get_irq(struct platform_device *, unsigned int);

void netif_carrier_off(struct net_device *dev);

int register_netdev(struct net_device *);
void unregister_netdev(struct net_device *);

void free_netdev(struct net_device *);

int device_init_wakeup(struct device *dev, bool val);

int regulator_disable(struct regulator *r);

void *dev_get_drvdata(const struct device *dev);

void netif_device_attach(struct net_device *);
void netif_device_detach(struct net_device *dev);

int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id);

#define SET_SYSTEM_SLEEP_PM_OPS(suspend_fn, resume_fn)
#define SET_RUNTIME_PM_OPS(suspend_fn, resume_fn, idle_fn)

struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	struct device_driver driver;
	const struct platform_device_id *id_table;
};

int platform_driver_register(struct platform_driver *);
void platform_driver_unregister(struct platform_driver *);

#define module_driver(__driver, __register, __unregister, ...) \
static int __init __driver##_init(void) \
{ \
	return __register(&(__driver) , ##__VA_ARGS__); \
} \
module_init(__driver##_init); \
static void __exit __driver##_exit(void) \
{ \
	__unregister(&(__driver) , ##__VA_ARGS__); \
} \
module_exit(__driver##_exit);

#define module_platform_driver(__platform_driver) \
	module_driver(__platform_driver, platform_driver_register, \
			platform_driver_unregister)

struct tasklet_struct
{
	void (*func)(unsigned long);
	unsigned long data;
};

#define PTR_ALIGN(p, a) ({              \
	unsigned long _p = (unsigned long)p;  \
	_p = (_p + a - 1) & ~(a - 1);         \
	p = (typeof(p))_p;                    \
	p;                                    \
})

struct kmem_cache;
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));
void   kmem_cache_destroy(struct kmem_cache *);
void  *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);
void  kmem_cache_free(struct kmem_cache *, void *);
void *kmalloc_node_track_caller(size_t size, gfp_t flags, int node);
void *kmem_cache_alloc_node(struct kmem_cache *s, gfp_t flags, int node);

struct mem_cgroup {};
typedef int possible_net_t;
typedef int rwlock_t;

bool gfp_pfmemalloc_allowed(gfp_t);

struct cred {
	struct user_namespace * user_ns;
};

struct file {
	const struct cred * f_cred;
};

struct net
{
	struct user_namespace * user_ns;
};

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

void local_bh_disable(void);
void local_bh_enable(void);

#include <uapi/linux/rtnetlink.h>
#include <uapi/linux/neighbour.h>

static inline int net_eq(const struct net *net1, const struct net *net2) {
	return net1 == net2; }

extern struct net init_net;

struct net *dev_net(const struct net_device *dev);

#define read_pnet(pnet) (&init_net)

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

struct page_counter
{
	atomic_long_t count;
	unsigned long limit;
};

struct cg_proto
{
	struct page_counter memory_allocated;
	struct percpu_counter sockets_allocated;
	int memory_pressure;
	long sysctl_mem[3];
};

void page_counter_charge(struct page_counter *counter, unsigned long nr_pages);
unsigned long page_counter_read(struct page_counter *counter);
void page_counter_uncharge(struct page_counter *counter, unsigned long nr_pages);

enum { UNDER_LIMIT, SOFT_LIMIT, OVER_LIMIT };

struct inode
{
	kuid_t        i_uid;
};

struct vm_area_struct;

void write_lock_bh(rwlock_t *);
void write_unlock_bh(rwlock_t *);

struct sock;
struct socket;

void security_sock_graft(struct sock *, struct socket *);

u32 prandom_u32(void);

void rcu_read_lock(void);
void rcu_read_unlock(void);

bool net_gso_ok(netdev_features_t features, int gso_type);

size_t copy_from_iter_nocache(void *addr, size_t bytes, struct iov_iter *i);

bool poll_does_not_wait(const poll_table *p);
void poll_wait(struct file *f, wait_queue_head_t *w, poll_table *p);

struct task_struct
{
	unsigned int          flags;
	struct page_frag      task_frag;
};

extern struct task_struct *current;

int in_softirq(void);

enum { MAX_SCHEDULE_TIMEOUT = 1000 };

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

#define write_pnet(pnet, net) do { (void)(net);} while (0)

#define smp_load_acquire(p)     *(p)
#define smp_store_release(p, v) *(p) = v;

size_t ksize(void *);

#define kmemcheck_bitfield_begin(name)
#define kmemcheck_bitfield_end(name)
#define kmemcheck_annotate_bitfield(ptr, name)
#define kmemcheck_annotate_variable(var)

struct page *virt_to_head_page(const void *x);

#define DEFINE_PER_CPU(type, name) \
	typeof(type) name
#define this_cpu_ptr(ptr) ptr

void *__alloc_page_frag(struct page_frag_cache *nc, unsigned int fragsz, gfp_t gfp_mask);

unsigned long local_irq_save(unsigned long flags);
void local_irq_restore(unsigned long);

int in_irq(void);

void trace_kfree_skb(struct sk_buff *, void *);
void trace_consume_skb(struct sk_buff *);

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

#define page_private(page)      ((page)->private)
#define set_page_private(page, v)  ((page)->private = (v))

void *kmap_atomic(struct page *page);
void kunmap_atomic(void *addr);

struct page *virt_to_page(const void *x);

struct pipe_inode_info;

ssize_t splice_to_pipe(struct pipe_inode_info *, struct splice_pipe_desc *);

extern const struct pipe_buf_operations nosteal_pipe_buf_ops;

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

void sg_mark_end(struct scatterlist *sg);
void sg_set_buf(struct scatterlist *, const void *, unsigned int);
void sg_set_page(struct scatterlist *, struct page *, unsigned int, unsigned int);

struct inet_skb_parm
{
	int iif;
};

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

enum { CAP_NET_RAW = 13 };

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

struct udphdr
{
	__sum16 check;
};

struct udphdr *udp_hdr(const struct sk_buff *skb);

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

#define skb_vlan_tag_present(__skb)  ((__skb)->vlan_tci & VLAN_TAG_PRESENT)

void vlan_set_encap_proto(struct sk_buff *skb, struct vlan_hdr *vhdr);

#define VLAN_CFI_MASK 0x1000 /* Canonical Format Indicator */
#define VLAN_TAG_PRESENT VLAN_CFI_MASK

int __vlan_insert_tag(struct sk_buff *skb, __be16 vlan_proto, u16 vlan_tci);

#define skb_vlan_tag_get(__skb)      ((__skb)->vlan_tci & ~VLAN_TAG_PRESENT)

void put_device(struct device *dev);

typedef void (*dr_release_t)(struct device *dev, void *res);
typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp);
void  devres_add(struct device *dev, void *res);
void  devres_free(void *res);
int devres_release(struct device *dev, dr_release_t release, dr_match_t match, void *match_data);

int dev_set_name(struct device *dev, const char *fmt, ...);

int  device_register(struct device *dev);

void device_del(struct device *dev);

int in_interrupt(void);

int of_driver_match_device(struct device *dev, const struct device_driver *drv);

struct device_attribute
{
	struct attribute attr;
};

#define __ATTRIBUTE_GROUPS(_name)                               \
	static const struct attribute_group *_name##_groups[] = {   \
		&_name##_group,                                         \
		NULL,                                                   \
	}

#define ATTRIBUTE_GROUPS(_name)                                 \
	static const struct attribute_group _name##_group = {       \
		.attrs = _name##_attrs,                                 \
	};                                                          \
__ATTRIBUTE_GROUPS(_name)


int sprintf(char *buf, const char *fmt, ...);

#define __ATTR_NULL { .attr = { .name = NULL } }

#define __ATTR_RO(name) __ATTR_NULL

#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)

int class_register(struct class *cls);
void class_unregister(struct class *cls);

int  bus_register(struct bus_type *bus);
void bus_unregister(struct bus_type *bus);

#define __stringify(x...) #x

int request_module(const char *name, ...);

void device_initialize(struct device *dev);

extern struct workqueue_struct *system_power_efficient_wq;

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
void free_irq(unsigned int irq, void *dev_id);
int enable_irq(unsigned int irq);
int disable_irq(unsigned int irq);
int disable_irq_nosync(unsigned int irq);

void netif_carrier_on(struct net_device *dev);

typedef struct {
	__u8 b[16];
} uuid_le;

int device_add(struct device *dev);
struct device *get_device(struct device *dev);

struct device *bus_find_device_by_name(struct bus_type *bus, struct device *start, const char *name);

void msleep(unsigned int);

int  device_bind_driver(struct device *dev);
void device_release_driver(struct device *dev);

struct device *class_find_device(struct class *cls, struct device *start, const void *data, int (*match)(struct device *, const void *));

struct device_node *of_get_next_available_child(const struct device_node *node, struct device_node *prev);

#define for_each_available_child_of_node(parent, child) \
	for (child = of_get_next_available_child(parent, NULL); child != NULL; \
	     child = of_get_next_available_child(parent, child))

int driver_register(struct device_driver *drv);
void driver_unregister(struct device_driver *drv);

int __init netdev_boot_setup(char *str);

static inline void eth_broadcast_addr(u8 *addr) {
	memset(addr, 0xff, ETH_ALEN); }

static inline void eth_zero_addr(u8 *addr) {
	memset(addr, 0x00, ETH_ALEN); }

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

static inline bool eth_proto_is_802_3(__be16 proto)
{
	proto &= htons(0xFF00);
	return (u16)proto >= (u16)htons(ETH_P_802_3_MIN);
}

static inline unsigned long compare_ether_header(const void *a, const void *b)
{
	u32 *a32 = (u32 *)((u8 *)a + 2);
	u32 *b32 = (u32 *)((u8 *)b + 2);

	return (*(u16 *)a ^ *(u16 *)b) | (a32[0] ^ b32[0]) |
	       (a32[1] ^ b32[1]) | (a32[2] ^ b32[2]);

}

bool netdev_uses_dsa(struct net_device *dev);

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

struct net_device *alloc_netdev_mqs(int sizeof_priv, const char *name, unsigned char name_assign_type, void (*setup)(struct net_device *), unsigned int txqs, unsigned int rxqs);

enum { NET_NAME_UNKNOWN = 0 };

int scnprintf(char *buf, size_t size, const char *fmt, ...);

void *skb_gro_header_fast(struct sk_buff *skb, unsigned int offset);
void *skb_gro_header_hard(struct sk_buff *skb, unsigned int hlen);
void *skb_gro_header_slow(struct sk_buff *skb, unsigned int hlen, unsigned int offset);
void skb_gro_pull(struct sk_buff *skb, unsigned int len);
void skb_gro_postpull_rcsum(struct sk_buff *skb, const void *start, unsigned int len);

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
struct packet_offload *gro_find_complete_by_type(__be16 type);

#define fs_initcall(x)

void dev_add_offload(struct packet_offload *po);

void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);

struct pm_qos_request {};

#define dma_wmb() __asm__ __volatile__ ("dmb oshst" : : : "memory")

#include <lx_emul/extern_c_end.h>

#endif /* _SRC__DRIVERS__NIC__FEC__LX_EMUL_H_ */
