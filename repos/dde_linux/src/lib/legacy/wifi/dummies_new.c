/*
 * \brief  Dummy functions
 * \author Josef Soentgen
 * \date   2018-04-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* local includes */
#include <lx_emul.h>

#if 0
#define TRACE \
		do { \
			lx_printf("%s not implemented from: %p\n", __func__, \
			          __builtin_return_address(0)); \
		} while (0)
#define TRACE_OK \
		do { \
			lx_printf("%s not implemented but OK\n", __func__); \
		} while (0)
#else
#define TRACE do { ; } while (0)
#define TRACE_OK do { ; } while (0)
#endif


void __dev_xdp_query(struct net_device *dev, bpf_op_t xdp_op, struct netdev_bpf *xdp)
{
	TRACE;
}


int __ethtool_get_link_ksettings(struct net_device *dev,
                                 struct ethtool_link_ksettings *link_ksettings)
{
	TRACE;
	return -1;
}


u32 __skb_get_hash_symmetric(const struct sk_buff *skb)
{
	TRACE;
	return 0;
}


// typedef int (*bpf_op_t)(struct net_device *dev, struct netdev_bpf *bpf);
// struct bpf_prog *bpf_prog_get_type(u32 ufd, enum bpf_prog_type type)
// {
// 	TRACE;
// 	return NULL;
// }


struct sk_buff **call_gro_receive(gro_receive_t cb, struct sk_buff **head, struct sk_buff *skb)
{
	TRACE;
	return NULL;
}


struct user_struct *current_user()
{
	TRACE;
	return NULL;
}


int dev_change_tx_queue_len(struct net_device *dev, unsigned long d)
{
	TRACE;
	return -1;
}


int dev_change_xdp_fd(struct net_device *dev, struct netlink_ext_ack *extack,
                      int fd, u32 flags)
{
	TRACE;
	return -1;
}


int dev_get_alias(const struct net_device *dev, char *p, size_t n)
{
	TRACE;
	return -1;
}


int dev_recursion_level(void)
{
	TRACE;
	return 0;
}


bool dev_validate_header(const struct net_device *dev, char *ll_header, int len)
{
	TRACE;
	return false;
}


void free_uid(struct user_struct *user)
{
	TRACE;
}


void iov_iter_revert(struct iov_iter *i, size_t bytes)
{
	TRACE;
}


kuid_t make_kuid(struct user_namespace *from, uid_t uid)
{
	TRACE;
	return 0;
}


void memalloc_noreclaim_restore(unsigned int flags)
{
	TRACE;
}


unsigned int memalloc_noreclaim_save(void)
{
	TRACE;
	return 0;
}


struct fq_flow *fq_flow_classify(struct fq *fq, struct fq_tin *tin,
                                 struct sk_buff *skb,
                                 fq_flow_get_default_t get_default_func)
{
	TRACE;
	return 0;
}



void fq_flow_init(struct fq_flow *flow)
{
	TRACE;
}


int fq_init(struct fq *fq, int flows_cnt)
{
	TRACE;
	return -1;
}


void fq_recalc_backlog(struct fq *fq, struct fq_tin *tin, struct fq_flow *flow)
{
	TRACE;
}


void fq_reset(struct fq *fq, fq_skb_free_t free_func)
{
	TRACE;
}


void fq_tin_enqueue(struct fq *fq, struct fq_tin *tin, struct sk_buff *skb,
                    fq_skb_free_t free_func, fq_flow_get_default_t get_default_func)
{
	TRACE;
}


void fq_tin_filter(struct fq *fq, struct fq_tin *tin, fq_skb_filter_t filter_func,
                   void *filter_data, fq_skb_free_t free_func)
{
	TRACE;
}


void fq_tin_init(struct fq_tin *tin)
{
	TRACE;
}


void fq_tin_reset(struct fq *fq, struct fq_tin *tin, fq_skb_free_t free_func)
{
	TRACE;
}


int irq_set_affinity_hint(unsigned int irq, const struct cpumask *m)
{
	TRACE;
	return 0;
}


struct net_device *netdev_master_upper_dev_get_rcu(struct net_device *dev)
{
	TRACE;
	return NULL;
}


void netdev_rss_key_fill(void *buffer, size_t len)
{
	TRACE;
	/* XXX get_random_once() to fill cmd.secret_key */
}


const void *of_get_mac_address(struct device_node *np)
{
	TRACE;
	return NULL;
}


struct device_node * pci_device_to_OF_node(const struct pci_dev *pdev)
{
	TRACE;
	return NULL;
}


int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
                          int minvec, int maxvec)
{
	TRACE;
	return -1;
}


int pci_find_ext_capability(struct pci_dev *dev, int cap)
{
	TRACE;
	return -1;
}


int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask)
{
	// TRACE_OK;
	return 0;
}


int pci_set_dma_mask(struct pci_dev *dev, u64 mask)
{
	// TRACE_OK;
	return 0;
}


struct pci_dev *pcie_find_root_port(struct pci_dev *dev)
{
	TRACE;
	return NULL;
}


int pcim_enable_device(struct pci_dev *pdev)
{
	TRACE;
	return 0;
}


bool pm_runtime_active(struct device *dev)
{
	TRACE;
	return true;
}


void pm_runtime_allow(struct device *dev)
{
	TRACE;
}


void pm_runtime_forbid(struct device *dev)
{
	TRACE;
}


int pm_runtime_get(struct device *dev)
{
	TRACE;
	return 1;
}


void pm_runtime_mark_last_busy(struct device *dev)
{
	TRACE;
}


int pm_runtime_put_autosuspend(struct device *dev)
{
	TRACE;
	return -1;
}


int pm_runtime_resume(struct device *dev)
{
	TRACE;
	return 1;
}


int pm_runtime_set_active(struct device *dev)
{
	TRACE;
	return 0;
}


void pm_runtime_set_autosuspend_delay(struct device *dev, int delay)
{
	TRACE;
}


bool pm_runtime_suspended(struct device *dev)
{
	// TRACE_OK;
	return false;
}


void pm_runtime_use_autosuspend(struct device *dev)
{
	TRACE;
}


void pm_wakeup_event(struct device *dev, unsigned int msec)
{
	TRACE;
}


void reuseport_detach_sock(struct sock *sk)
{
	TRACE;
}


int sg_nents(struct scatterlist *sg)
{
	TRACE;
	return -1;
}


size_t sg_pcopy_from_buffer(struct scatterlist *sgl, unsigned int nents,
                            const void *buf, size_t buflen, off_t skip)
{
	TRACE;
	return 0;
}


int sk_reuseport_attach_bpf(u32 ufd, struct sock *sk)
{
	TRACE;
	return -1;
}


int sk_reuseport_attach_filter(struct sock_fprog *fprog, struct sock *sk)
{
	TRACE;
	return -1;
}


void skb_gro_flush_final(struct sk_buff *skb, struct sk_buff **pp, int flush)
{
	TRACE;
}


void *skb_gro_header_fast(struct sk_buff *skb, unsigned int offset)
{
	TRACE;
	return NULL;
}


int skb_gro_header_hard(struct sk_buff *skb, unsigned int hlen)
{
	TRACE;
	return 0; // XXX
}


void *skb_gro_header_slow(struct sk_buff *skb, unsigned int hlen, unsigned int offset)
{
	TRACE;
	return NULL;
}


void skb_gro_postpull_rcsum(struct sk_buff *skb, const void *start, unsigned int len)
{
	TRACE;
}


void skb_gro_pull(struct sk_buff *skb, unsigned int len)
{
	TRACE;
}


u64 sock_gen_cookie(struct sock *sk)
{
	TRACE;
	return 0;
}


unsigned int u64_stats_fetch_begin(const struct u64_stats_sync *syncp)
{
	// TRACE;
	return 0;
}


unsigned int u64_stats_fetch_begin_irq(const struct u64_stats_sync *syncp)
{
	// TRACE;
	return 0;
}


bool u64_stats_fetch_retry(const struct u64_stats_sync *syncp, unsigned int start)
{
	// TRACE;
	return false;
}


bool u64_stats_fetch_retry_irq(const struct u64_stats_sync *syncp, unsigned int start)
{
	// TRACE;
	return false;
}


void u64_stats_init(struct u64_stats_sync *syncp)
{
	// TRACE;
}


struct sk_buff *validate_xmit_skb_list(struct sk_buff *skb, struct net_device *dev,
                                       bool *again)
{
	TRACE;
	return NULL;
}

int virtio_net_hdr_from_skb(const struct sk_buff *skb, struct virtio_net_hdr *hdr,
                            bool little_endian, bool has_data_valid)
{
	TRACE;
	return -1;
}


int virtio_net_hdr_to_skb(struct sk_buff *skb, const struct virtio_net_hdr *hdr,
                          bool little_endian)
{
	TRACE;
	return -1;
}


void wireless_nlevent_flush(void)
{
	TRACE;
}


bool wq_has_sleeper(struct wait_queue_head *wq_head)
{
	TRACE_OK;
	return true;
}


bool sysfs_streq(const char *s1, const char *s2)
{
	TRACE;
	return false;
}
