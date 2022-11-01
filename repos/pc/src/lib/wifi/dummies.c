/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Josef Soentgen
 * \date   2022-02-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


const unsigned long module_cert_size = 0;
const u8 system_certificate_list[] = { };
const unsigned long system_certificate_list_size = sizeof (system_certificate_list);

const u8 shipped_regdb_certs[] = { };
unsigned int shipped_regdb_certs_len = sizeof (shipped_regdb_certs);



#include <linux/filter.h>
#include <linux/jump_label.h> /* for DEFINE_STATIC_KEY_FALSE */

void bpf_prog_change_xdp(struct bpf_prog *prev_prog, struct bpf_prog *prog)
{
	lx_emul_trace(__func__);
}

DEFINE_STATIC_KEY_FALSE(bpf_stats_enabled_key);


asmlinkage __wsum csum_partial(const void * buff,int len,__wsum sum)
{
	lx_emul_trace_and_stop(__func__);
}


struct static_key_false init_on_alloc;


#include <linux/proc_ns.h>

int proc_alloc_inum(unsigned int * inum)
{
	*inum = 1; /* according to linux/proc_ns.h without CONFIG_PROC_FS */
	return 0;
}


#include <net/net_namespace.h>

__init int net_sysctl_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/fs.h>

unsigned int get_next_ino(void)
{
	static unsigned int count = 0;
	return ++count;
}


#include <linux/netdevice.h>

int __init dev_proc_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/stringhash.h>

unsigned int full_name_hash(const void * salt,const char * name,unsigned int len)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/key.h>

static struct key _key;

struct key * keyring_alloc(const char * description,kuid_t uid,kgid_t gid,const struct cred * cred,key_perm_t perm,unsigned long flags,struct key_restriction * restrict_link,struct key * dest)
{
	lx_emul_trace(__func__);
	return &_key;
}


#include <linux/kobject.h>

int kobject_uevent_env(struct kobject * kobj,enum kobject_action action,char * envp_ext[])
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/moduleparam.h>

void kernel_param_lock(struct module * mod)
{
	lx_emul_trace(__func__);
}


#include <linux/moduleparam.h>

void kernel_param_unlock(struct module * mod)
{
	lx_emul_trace(__func__);
}


#include <linux/pid.h>

void put_pid(struct pid * pid)
{
	lx_emul_trace(__func__);
}


#include <linux/filter.h>

int sk_filter_trim_cap(struct sock * sk,struct sk_buff * skb,unsigned int cap)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/capability.h>

bool file_ns_capable(const struct file * file,struct user_namespace * ns,int cap)
{
	lx_emul_trace(__func__);
	return true;
}


#include <linux/rcupdate.h>

void synchronize_rcu(void)
{
	lx_emul_trace(__func__);
}


#include <linux/skbuff.h>

void __skb_get_hash(struct sk_buff * skb)
{
	lx_emul_trace(__func__);
}


#include <linux/skbuff.h>

bool __skb_flow_dissect(const struct net * net,const struct sk_buff * skb,struct flow_dissector * flow_dissector,void * target_container,const void * data,__be16 proto,int nhoff,int hlen,unsigned int flags)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/pid.h>

pid_t pid_vnr(struct pid * pid)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/verification.h>

int verify_pkcs7_signature(const void *data, size_t len,
               const void *raw_pkcs7, size_t pkcs7_len,
               struct key *trusted_keys,
               enum key_being_used_for usage,
               int (*view_content)(void *ctx,
                           const void *data, size_t len,
                           size_t asn1hdrlen),
               void *ctx)
{
	return true;
}


#include <linux/acpi.h>
#include <acpi/acpi.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpixf.h>


acpi_status acpi_evaluate_object(acpi_handle handle,
             acpi_string pathname,
             struct acpi_object_list *external_params,
             struct acpi_buffer *return_buffer)
{
	lx_emul_trace_and_stop(__func__);
}


acpi_status acpi_get_handle(acpi_handle parent,acpi_string pathname,acpi_handle * ret_handle)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

const struct attribute_group pci_dev_acpi_attr_group;

struct irq_domain *pci_host_bridge_acpi_msi_domain(struct pci_bus *bus)
{
	return NULL;
}


bool pciehp_is_native(struct pci_dev *bridge)
{
	return true;
}


void pci_lock_rescan_remove(void)
{
	lx_emul_trace(__func__);
}


void pci_unlock_rescan_remove(void)
{
	lx_emul_trace(__func__);
}


bool pci_pme_capable(struct pci_dev *dev, pci_power_t state)
{
	lx_emul_trace(__func__);
	return false;
}


int pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val)
{
	lx_emul_trace(__func__);
	return -1;
}


u16 pci_find_ext_capability(struct pci_dev *dev, int cap)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/thermal.h>

struct thermal_cooling_device *thermal_cooling_device_register(const char *s,
	void *p, const struct thermal_cooling_device_ops *op)
{
	return ERR_PTR(-ENODEV);
}


void thermal_cooling_device_unregister(struct thermal_cooling_device *tcd)
{
	lx_emul_trace(__func__);
}


int thermal_zone_device_enable(struct thermal_zone_device *tz)
{
	return -ENODEV;
}


struct thermal_zone_device *thermal_zone_device_register(const char *s, int i, int j,
        void *p, struct thermal_zone_device_ops *ops,
        struct thermal_zone_params *params, int x, int y)
{
	return ERR_PTR(-ENODEV);
}


void thermal_zone_device_unregister(struct thermal_zone_device *tzd)
{
	lx_emul_trace(__func__);
}


void thermal_zone_device_update(struct thermal_zone_device *tzd,
                enum thermal_notify_event e)
{
	lx_emul_trace(__func__);
}


#include <linux/net.h>

int net_ratelimit(void)
{
	lx_emul_trace(__func__);
	/* suppress */
	return 0;
}


#include <asm/smp.h>

struct smp_ops smp_ops = { };
EXPORT_SYMBOL_GPL(smp_ops);


void synchronize_rcu_expedited(void)
{
	lx_emul_trace(__func__);
}


int pci_enable_msi(struct pci_dev *dev)
{
	lx_emul_trace(__func__);
	return -ENOSYS;
}


int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
                          int minvec, int maxvec)
{
	lx_emul_trace(__func__);
	return -ENOSYS;
}
