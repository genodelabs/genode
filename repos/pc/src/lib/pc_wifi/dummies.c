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


#include <lx_emul/task.h>

void yield(void)
{
	lx_emul_task_schedule(0 /* no block */);
}


#include <linux/once.h>

void __do_once_done(bool * done,struct static_key_true * once_key,unsigned long * flags,struct module * mod)
{
	*done = true;
}


#include <linux/once.h>

bool __do_once_start(bool * done,unsigned long * flags)
{
	return !*done;
}


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


#ifdef CONFIG_X86_32
asmlinkage __wsum csum_partial(const void * buff,int len,__wsum sum)
{
	lx_emul_trace_and_stop(__func__);
}
#endif /* CONFIG_X86_32 */


#ifdef CONFIG_X86_64
extern unsigned long arch_scale_cpu_capacity(int cpu);
unsigned long arch_scale_cpu_capacity(int cpu)
{
	lx_emul_trace_and_stop(__func__);
}
#endif /* CONFIG_X86_64 */


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


#include <linux/fs.h>

void inode_init_once(struct inode * inode)
{
	lx_emul_trace(__func__);
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

void __skb_get_hash_net(const struct net * net,struct sk_buff * skb)
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


acpi_status acpi_get_handle(acpi_handle parent,char const* pathname,acpi_handle * ret_handle)
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


#include <linux/pci.h>

int pci_enable_msi(struct pci_dev *dev)
{
	lx_emul_trace(__func__);
	return -ENOSYS;
}


void pci_disable_msi(struct pci_dev *dev)
{
    lx_emul_trace(__func__);
}


int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
                          int minvec, int maxvec)
{
	lx_emul_trace(__func__);
	return -ENOSYS;
}


void pci_disable_device(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


#include <linux/leds.h>
#include <../net/mac80211/ieee80211_i.h> /* struct ieee80211_local */

void ieee80211_free_led_names(struct ieee80211_local * local)
{
	lx_emul_trace(__func__);
}


#include <linux/pgtable.h>

pteval_t __default_kernel_pte_mask __read_mostly = ~0;


u16 get_random_u16(void)
{
	lx_emul_trace_and_stop(__func__);
}


u8 get_random_u8(void)
{
	lx_emul_trace_and_stop(__func__);
}


DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);
EXPORT_PER_CPU_SYMBOL(cpu_sibling_map);


#include <linux/filter.h>

DEFINE_STATIC_KEY_FALSE(bpf_master_redirect_enabled_key);
EXPORT_SYMBOL_GPL(bpf_master_redirect_enabled_key);


extern const struct attribute_group dev_attr_physical_location_group;
const struct attribute_group dev_attr_physical_location_group = {};


#include <linux/acpi.h>

void acpi_device_notify(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern bool dev_add_physical_location(struct device * dev);
bool dev_add_physical_location(struct device * dev)
{
	lx_emul_trace(__func__);
	return false;
}


#include <net/gen_stats.h>

void gnet_stats_basic_sync_init(struct gnet_stats_basic_sync * b)
{
	lx_emul_trace(__func__);
}


#include <linux/iommu.h>

int iommu_device_use_default_domain(struct device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_enter(void)
{
	lx_emul_trace(__func__);
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_exit(void)
{
	lx_emul_trace(__func__);
}


unsigned int pci_rescan_bus(struct pci_bus *bus)
{
	lx_emul_trace(__func__);
	return 0;
}


void pcim_pin_device(struct pci_dev *pdev)
{
	lx_emul_trace(__func__);
}


void pcim_iounmap(struct pci_dev *pdev, void __iomem *addr)
{
	lx_emul_trace(__func__);
}


#include <linux/sysctl.h>

void __init __register_sysctl_init(const char * path,struct ctl_table * table,const char * table_name, size_t table_size)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

int sysfs_add_file_to_group(struct kobject * kobj,const struct attribute * attr,const char * group)
{
	lx_emul_trace(__func__);
	return 0;
}


void * high_memory;


int pcim_iomap_regions(struct pci_dev *pdev, int mask, const char *name)
{
	lx_emul_trace(__func__);
	return 0;
}


int pcie_capability_clear_and_set_word_locked(struct pci_dev *dev, int pos,
                                              u16 clear, u16 set)
{
	lx_emul_trace(__func__);
	return 0;
}


int pcie_capability_clear_and_set_word_unlocked(struct pci_dev *dev, int pos,
                                                u16 clear, u16 set)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/cdev.h>

void cdev_init(struct cdev * cdev,const struct file_operations * fops)
{
	lx_emul_trace(__func__);
}


#include <crypto/algapi.h>

/*
 * For the moment implement here as the it will otherwise clash with
 * older kernel versions, 5.14.x on the PinePhone, where it is implmented
 * in 'crypto/algapi.c.
 */
void __crypto_xor(u8 *dst, const u8 *src1, const u8 *src2, unsigned int len)
{
	while (len--)
		*dst++ = *src1++ ^ *src2++;
}


#include <../drivers/net/wireless/intel/iwlwifi/iwl-trans.h>
#include <../drivers/net/wireless/intel/iwlwifi/fw/uefi.h>

void *iwl_uefi_get_pnvm(struct iwl_trans *trans, size_t *len)
{
	lx_emul_trace(__func__);
	return ERR_PTR(-EOPNOTSUPP);
}


u8 *iwl_uefi_get_reduced_power(struct iwl_trans *trans, size_t *len)
{
	lx_emul_trace(__func__);
	return ERR_PTR(-EOPNOTSUPP);
}


void iwl_uefi_get_sgom_table(struct iwl_trans *trans, struct iwl_fw_runtime *fwrt)
{
	lx_emul_trace(__func__);
}


void iwl_uefi_get_step_table(struct iwl_trans * trans)
{
	lx_emul_trace(__func__);
}


int iwl_uefi_handle_tlv_mem_desc(struct iwl_trans * trans,const u8 * data,u32 tlv_len,struct iwl_pnvm_image * pnvm_data)
{
	return -EINVAL;
}


int iwl_uefi_reduce_power_parse(struct iwl_trans * trans,const u8 * data,size_t len,struct iwl_pnvm_image * pnvm_data)
{
	return -ENOENT;
}

#include <fw/regulatory.h>

int iwl_bios_get_dsm(struct iwl_fw_runtime * fwrt,enum iwl_dsm_funcs func,u32 * value) { return -ENOENT; }
int iwl_bios_get_eckv(struct iwl_fw_runtime *fwrt, u32 * data) { return -ENOENT; }
int iwl_bios_get_mcc(struct iwl_fw_runtime *fwrt, char * data) { return -ENOENT; }
int iwl_bios_get_pwr_limit(struct iwl_fw_runtime *fwrt, u64 * data) { return -ENOENT; }
int iwl_bios_get_tas_table(struct iwl_fw_runtime *fwrt, struct iwl_tas_data * data) { return -ENOENT; }
int iwl_bios_get_wbem(struct iwl_fw_runtime *fwrt, u32 * data) { return -ENOENT; }
int iwl_bios_get_ewrd_table(struct iwl_fw_runtime *fwrt) { return -ENOENT; }
int iwl_bios_get_ppag_table(struct iwl_fw_runtime *fwrt) { return -ENOENT; }
int iwl_bios_get_wgds_table(struct iwl_fw_runtime *fwrt) { return -ENOENT; }
int iwl_bios_get_wrds_table(struct iwl_fw_runtime *fwrt) { return -ENOENT; }


extern int iwl_fill_lari_config(struct iwl_fw_runtime * fwrt,struct iwl_lari_config_change_cmd * cmd,size_t * cmd_size);
int iwl_fill_lari_config(struct iwl_fw_runtime * fwrt,struct iwl_lari_config_change_cmd * cmd,size_t * cmd_size)
{
	lx_emul_trace(__func__);
	return 1;
}


extern int iwl_fill_ppag_table(struct iwl_fw_runtime * fwrt,union iwl_ppag_table_cmd * cmd,int * cmd_size);
int iwl_fill_ppag_table(struct iwl_fw_runtime * fwrt,union iwl_ppag_table_cmd * cmd,int * cmd_size)
{
	lx_emul_trace(__func__);
	return -1;
}

extern bool iwl_is_ppag_approved(struct iwl_fw_runtime * fwrt);
bool iwl_is_ppag_approved(struct iwl_fw_runtime * fwrt)
{
	lx_emul_trace(__func__);
	return false;
}


extern bool iwl_is_tas_approved(void);
bool iwl_is_tas_approved(void)
{
	lx_emul_trace(__func__);
	return false;
}


extern int iwl_sar_fill_profile(struct iwl_fw_runtime * fwrt,__le16 * per_chain,u32 n_tables,u32 n_subbands,int prof_a,int prof_b);
int iwl_sar_fill_profile(struct iwl_fw_runtime * fwrt,__le16 * per_chain,u32 n_tables,u32 n_subbands,int prof_a,int prof_b)
{
	lx_emul_trace(__func__);
	/* means profil is disabled */
	return 1;
}


extern int iwl_sar_geo_fill_table(struct iwl_fw_runtime * fwrt,struct iwl_per_chain_offset * table,u32 n_bands,u32 n_profiles);
int iwl_sar_geo_fill_table(struct iwl_fw_runtime * fwrt,struct iwl_per_chain_offset * table,u32 n_bands,u32 n_profiles)
{
	lx_emul_trace_and_stop(__func__);
	/* means not supported */
	return 1;
}


extern bool iwl_sar_geo_support(struct iwl_fw_runtime * fwrt);
bool iwl_sar_geo_support(struct iwl_fw_runtime * fwrt)
{
	lx_emul_trace(__func__);
	return false;
}


extern int iwl_uefi_get_puncturing(struct iwl_fw_runtime * fwrt);
int iwl_uefi_get_puncturing(struct iwl_fw_runtime * fwrt)
{
	lx_emul_trace(__func__);
	return 0;
}


extern bool iwl_puncturing_is_allowed_in_bios(u32 puncturing,u16 mcc);
bool iwl_puncturing_is_allowed_in_bios(u32 puncturing,u16 mcc)
{
	lx_emul_trace(__func__);
	return false;
}


extern int iwl_uefi_get_uats_table(struct iwl_trans * trans,struct iwl_fw_runtime * fwrt);
int iwl_uefi_get_uats_table(struct iwl_trans * trans,struct iwl_fw_runtime * fwrt)
{
	lx_emul_trace(__func__);
	return -1;
}



#include <linux/property.h>

int software_node_notify(struct device * dev,unsigned long action)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/leds.h>

int led_trigger_register(struct led_trigger * trig)
{
	lx_emul_trace(__func__);
	return 0;
}


void led_trigger_event(struct led_trigger * trig,enum led_brightness brightness)
{
	lx_emul_trace(__func__);
}



extern void ieee80211_alloc_led_names(struct ieee80211_local * local);
void ieee80211_alloc_led_names(struct ieee80211_local * local)
{
	lx_emul_trace(__func__);
}


extern void ieee80211_led_assoc(struct ieee80211_local * local,bool associated);
void ieee80211_led_assoc(struct ieee80211_local * local,bool associated)
{
	lx_emul_trace(__func__);
}


extern void ieee80211_led_init(struct ieee80211_local * local);
void ieee80211_led_init(struct ieee80211_local * local)
{
	lx_emul_trace(__func__);
}


extern void ieee80211_led_radio(struct ieee80211_local * local,bool enabled);
void ieee80211_led_radio(struct ieee80211_local * local,bool enabled)
{
	lx_emul_trace(__func__);
}


extern void ieee80211_mod_tpt_led_trig(struct ieee80211_local * local,unsigned int types_on,unsigned int types_off);
void ieee80211_mod_tpt_led_trig(struct ieee80211_local * local,unsigned int types_on,unsigned int types_off)
{
	lx_emul_trace(__func__);
}


extern int iwl_mvm_leds_init(struct iwl_mvm * mvm);
int iwl_mvm_leds_init(struct iwl_mvm * mvm)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void iwl_mvm_leds_sync(struct iwl_mvm * mvm);
void iwl_mvm_leds_sync(struct iwl_mvm * mvm)
{
	lx_emul_trace(__func__);
}


#include <linux/pinctrl/devinfo.h>

int pinctrl_bind_pins(struct device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/pinctrl/devinfo.h>

int pinctrl_init_done(struct device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void iwl_leds_exit(struct iwl_priv * priv);
void iwl_leds_exit(struct iwl_priv * priv)
{
	lx_emul_trace(__func__);
}


extern void iwl_leds_init(struct iwl_priv * priv);
void iwl_leds_init(struct iwl_priv * priv)
{
	lx_emul_trace(__func__);
}


extern void iwl_mvm_leds_exit(struct iwl_mvm * mvm);
void iwl_mvm_leds_exit(struct iwl_mvm * mvm)
{
	lx_emul_trace(__func__);
}


extern void iwlagn_led_enable(struct iwl_priv * priv);
void iwlagn_led_enable(struct iwl_priv * priv)
{
	lx_emul_trace(__func__);
}


#include <linux/iommu.h>

void iommu_device_unuse_default_domain(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <net/mac80211.h>

const char * __ieee80211_create_tpt_led_trigger(struct ieee80211_hw * hw,unsigned int flags,const struct ieee80211_tpt_blink * blink_table,unsigned int blink_table_len)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <net/mac80211.h>

const char * __ieee80211_get_radio_led_name(struct ieee80211_hw * hw)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <linux/leds.h>

int led_classdev_register_ext(struct device * parent,struct led_classdev * led_cdev,struct led_init_data * init_data)
{
	lx_emul_trace(__func__);
	return -1;
}


#include <linux/leds.h>

void led_classdev_unregister(struct led_classdev * led_cdev)
{
	lx_emul_trace(__func__);
}


#include <linux/leds.h>

void led_trigger_blink_oneshot(struct led_trigger * trig,unsigned long delay_on,unsigned long delay_off,int invert)
{
	lx_emul_trace(__func__);
}


#include <linux/leds.h>

void led_trigger_unregister(struct led_trigger * trig)
{
	lx_emul_trace(__func__);
}


#include <linux/mnt_idmapping.h>

struct mnt_idmap { unsigned dummy; };
struct mnt_idmap nop_mnt_idmap;


#include <linux/thermal.h>

struct thermal_zone_device *thermal_zone_device_register_with_trips(
	const char *type,
	const struct thermal_trip *trips,
	int num_trips, void *devdata,
	const struct thermal_zone_device_ops *ops,
	const struct thermal_zone_params *tzp,
	unsigned int passive_delay,
	unsigned int polling_delay)
{
	lx_emul_trace(__func__);
	return ERR_PTR(-EINVAL);
}


#include <linux/ratelimit_types.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
{
	lx_emul_trace(__func__);
	return 0;
}

#include <linux/async.h>

void __init async_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rcutree.h>

void kvfree_rcu_barrier(void)
{
	/*
	 * The way kvfree_call_rcu is currently implemented should make
	 * this function just being a NOP workable (until it does not...).
	 */
	lx_emul_trace(__func__);
}


/*
 * Disable WBRF support at the stack layer because we do not
 * currently do not have a way to call ACPI.
 */

#include <net/cfg80211.h>

extern void ieee80211_check_wbrf_support(struct ieee80211_local * local);
void ieee80211_check_wbrf_support(struct ieee80211_local * local)
{
	lx_emul_trace(__func__);
}


extern void ieee80211_add_wbrf(struct ieee80211_local * local,struct cfg80211_chan_def * chandef);
void ieee80211_add_wbrf(struct ieee80211_local * local,struct cfg80211_chan_def * chandef)
{
	lx_emul_trace(__func__);
}


extern void ieee80211_remove_wbrf(struct ieee80211_local * local,struct cfg80211_chan_def * chandef);
void ieee80211_remove_wbrf(struct ieee80211_local * local,struct cfg80211_chan_def * chandef)
{
	lx_emul_trace(__func__);
}


/*
 * The dummies below trigger when the proper firmware ucode
 * could not be loaded and the driver gets shut down. They
 * are solely there for preventing 'not implemented yet!' messages
 * in this case.
 */

extern void unregister_handler_proc(unsigned int irq,struct irqaction * action);
void unregister_handler_proc(unsigned int irq,struct irqaction * action)
{
	lx_emul_trace(__func__);
}


#include <linux/task_work.h>

struct callback_head * task_work_cancel_func(struct task_struct * task,task_work_func_t func)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <linux/rcuwait.h>

void finish_rcuwait(struct rcuwait * w)
{
	lx_emul_trace(__func__);
}


/*
 * The dummies below were moved from the generated dummies
 * file to provide the proper forward declarations and
 * thus still feature 'lx_emul_trace_and_stop'.
 */

#include <crypto/skcipher.h>

int crypto_lskcipher_setkey(struct crypto_lskcipher * tfm,const u8 * key,unsigned int keylen)
{
	lx_emul_trace_and_stop(__func__);
}


extern int crypto_init_lskcipher_ops_sg(struct crypto_tfm * tfm);
int crypto_init_lskcipher_ops_sg(struct crypto_tfm * tfm)
{
	lx_emul_trace_and_stop(__func__);
}


extern int crypto_lskcipher_decrypt_sg(struct skcipher_request * req);
int crypto_lskcipher_decrypt_sg(struct skcipher_request * req)
{
	lx_emul_trace_and_stop(__func__);
}


extern int crypto_lskcipher_encrypt_sg(struct skcipher_request * req);
int crypto_lskcipher_encrypt_sg(struct skcipher_request * req)
{
	lx_emul_trace_and_stop(__func__);
}
