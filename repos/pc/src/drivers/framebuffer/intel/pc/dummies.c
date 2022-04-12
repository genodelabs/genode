/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Alexander Boettcher
 * \date   2022-01-21
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/debug.h>
#include <linux/math64.h>

#include "i915_drv.h"

#include <acpi/acpixf.h>
#include <acpi/video.h>
#include <linux/clocksource.h>
#include <linux/cpuhotplug.h>
#include <linux/kernel_stat.h>
#include <linux/kernfs.h>
#include <linux/kobject.h>
#include <linux/nls.h>
#include <linux/property.h>
#include <linux/random.h>
#include <linux/rcupdate.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/signal.h>
#include <linux/syscore_ops.h>
#include <linux/timekeeper_internal.h>


unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)];
unsigned long lpj_fine = 0;


const guid_t pci_acpi_dsm_guid =
	GUID_INIT(0xe5c937d0, 0x3553, 0x4d7a,
		  0x91, 0x17, 0xea, 0x4d, 0x19, 0xc3, 0x43, 0x4d);


void rcu_barrier(void)
{
	lx_emul_trace_and_stop(__func__);
}


int __cpuhp_setup_state(enum cpuhp_state state,const char * name,bool invoke,int (* startup)(unsigned int cpu),int (* teardown)(unsigned int cpu),bool multi_instance)
{
	lx_emul_trace(__func__);
	return 0;
}


void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}


void clocksource_arch_init(struct clocksource * cs)
{
	lx_emul_trace(__func__);
}


void ignore_signals(struct task_struct * t)
{
	lx_emul_trace(__func__);
}


void calc_global_load(void)
{
	lx_emul_trace(__func__);
}


void account_process_tick(struct task_struct * p,int user_tick)
{
	lx_emul_trace(__func__);
}


void rcu_sched_clock_irq(int user)
{
	lx_emul_trace(__func__);
}


void kernfs_get(struct kernfs_node * kn)
{
	lx_emul_trace(__func__);
}


void kernfs_put(struct kernfs_node * kn)
{
	lx_emul_trace(__func__);
}


int software_node_notify(struct device * dev,unsigned long action)
{
	lx_emul_trace(__func__);
	return 0;
}


void get_random_bytes(void * buf,int nbytes)
{
	lx_emul_trace(__func__);
}


int __must_check get_random_bytes_arch(void * buf,int nbytes)
{
	lx_emul_trace(__func__);
	return 0;
}


int add_random_ready_callback(struct random_ready_callback * rdy)
{
	lx_emul_trace(__func__);
	return 0;
}


void add_interrupt_randomness(int irq,int irq_flags)
{
	lx_emul_trace(__func__);
}


int sysfs_create_files(struct kobject * kobj,const struct attribute * const * ptr)
{
	lx_emul_trace(__func__);
	return 0;
}


int sysfs_create_bin_file(struct kobject * kobj,const struct bin_attribute * attr)
{
	lx_emul_trace(__func__);
	return 0;
}


void sysfs_remove_file_ns(struct kobject * kobj,const struct attribute * attr,const void * ns)
{
	lx_emul_trace(__func__);
}


int sysfs_create_dir_ns(struct kobject * kobj,const void * ns)
{
	lx_emul_trace(__func__);
	return 0;
}


int sysfs_create_file_ns(struct kobject * kobj,const struct attribute * attr,const void * ns)
{
	lx_emul_trace(__func__);
	return 0;
}


int sysfs_create_link(struct kobject * kobj,struct kobject * target,const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


int sysfs_create_groups(struct kobject * kobj,const struct attribute_group ** groups)
{
	lx_emul_trace(__func__);
	return 0;
}


void sysfs_remove_link(struct kobject * kobj,const char * name)
{
	lx_emul_trace(__func__);
}


void sysfs_remove_groups(struct kobject * kobj,const struct attribute_group ** groups)
{
	lx_emul_trace(__func__);
}


void sysfs_remove_dir(struct kobject * kobj)
{
	lx_emul_trace(__func__);
}


void sysfs_remove_bin_file(struct kobject * kobj,const struct bin_attribute * attr)
{
	lx_emul_trace(__func__);
}


bool sysfs_remove_file_self(struct kobject * kobj,const struct attribute * attr)
{
	lx_emul_trace(__func__);
	return false;
}


void sysfs_remove_files(struct kobject * kobj,const struct attribute * const * ptr)
{
	lx_emul_trace(__func__);
}


void sysfs_delete_link(struct kobject * kobj,struct kobject * targ,const char * name)
{
	lx_emul_trace(__func__);
}


int sysfs_emit(char * buf,const char * fmt,...)
{
	lx_emul_trace(__func__);
	return PAGE_SIZE;
}


int sysfs_emit_at(char * buf, int at, const char * fmt,...)
{
	lx_emul_trace(__func__);
	return at > PAGE_SIZE ? PAGE_SIZE : PAGE_SIZE - at;
}


void sysfs_notify(struct kobject * kobj,const char * dir,const char * attr)
{
	lx_emul_trace(__func__);
}



void intel_engines_add_sysfs(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}



void register_syscore_ops(struct syscore_ops * ops)
{
	wait_bit_init();
	lx_emul_trace(__func__);
}


void pci_allocate_vc_save_buffers(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


void pci_vpd_init(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


int pci_proc_attach_device(struct pci_dev * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


void i915_pmu_init(void)
{
	lx_emul_trace(__func__);
}


void i915_pmu_register(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


bool parse_option_str(const char *str, const char *option)
{
	lx_emul_trace(__func__);
	return false;
}


int __register_chrdev(unsigned int major,unsigned int baseminor,unsigned int count,const char * name,const struct file_operations * fops)
{
	lx_emul_trace(__func__);
	return 0;
}


void intel_gt_driver_late_release(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


bool intel_vgpu_active(struct drm_i915_private * dev_priv)
{
	lx_emul_trace(__func__);
	return false;
}


void unregister_shrinker(struct shrinker * shrinker)
{
	lx_emul_trace(__func__);
}


int register_shrinker(struct shrinker * shrinker)
{
	lx_emul_trace(__func__);
	return 0;
}


int intel_gt_init_mmio(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
	return 0;
}


int __intel_gt_reset(struct intel_gt * gt, intel_engine_mask_t engine_mask)
{
	lx_emul_trace(__func__);
	return 0;
}


void i915_perf_init(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


int drm_aperture_remove_conflicting_pci_framebuffers(struct pci_dev * pdev,
                                                     const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


void intel_gt_init_hw_early(struct intel_gt * gt, struct i915_ggtt * ggtt)
{
	lx_emul_trace(__func__);
}


int intel_gt_probe_lmem(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
	return 0;
}


void intel_gt_init_workarounds(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


void sched_set_fifo(struct task_struct * p)
{
	lx_emul_trace(__func__);
}


void gen8_gt_irq_handler(struct intel_gt * gt,u32 master_ctl)
{
	lx_emul_trace(__func__);
}


void gen11_gt_irq_reset(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen11_gt_irq_handler(struct intel_gt * gt, const u32 master_ctl)
{
	lx_emul_trace(__func__);
}


void gen11_gt_irq_postinstall(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen8_gt_irq_reset(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen8_gt_irq_postinstall(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen5_gt_irq_reset(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen5_gt_irq_postinstall(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void register_irq_proc(unsigned int irq,struct irq_desc * desc)
{
	lx_emul_trace(__func__);
}


void register_handler_proc(unsigned int irq,struct irqaction * action)
{
	lx_emul_trace(__func__);
}


void unregister_handler_proc(unsigned int irq,struct irqaction * action)
{
	lx_emul_trace(__func__);
}


void intel_vgpu_register(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


void i915_perf_register(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


void intel_gt_driver_register(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void i915_perf_sysctl_register(void)
{
	lx_emul_trace(__func__);
}


int acpi_platform_notify(struct device *dev, enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}


bool acpi_driver_match_device(struct device *dev, const struct device_driver *drv)
{
	lx_emul_trace(__func__);
	return false;
}


bool is_acpi_device_node(const struct fwnode_handle *fwnode)
{
	lx_emul_trace(__func__);
	return false;
}


enum dev_dma_attr acpi_get_dma_attr(struct acpi_device *adev)
{
	lx_emul_trace_and_stop(__func__);
}


int acpi_dma_configure_id(struct device *dev, enum dev_dma_attr attr, const u32 *input_id)
{
	lx_emul_trace_and_stop(__func__);
}


int acpi_device_uevent_modalias(struct device *dev, struct kobj_uevent_env *event)
{
	lx_emul_trace_and_stop(__func__);
}


int acpi_device_modalias(struct device *device, char * x, int y)
{
	lx_emul_trace_and_stop(__func__);
}


void acpi_video_unregister(void)
{
	lx_emul_trace_and_stop(__func__);
}


bool acpi_check_dsm(acpi_handle handle, const guid_t *guid, u64 rev, u64 funcs)
{
	lx_emul_trace_and_stop(__func__);
}


union acpi_object *acpi_evaluate_dsm(acpi_handle handle, const guid_t *guid,
                                     u64 rev, u64 func, union acpi_object *argv4)
{
	lx_emul_trace_and_stop(__func__);
}


acpi_status acpi_remove_address_space_handler(acpi_handle device,
                                              acpi_adr_space_type space_id,
                                              acpi_adr_space_handler handler)
{
	lx_emul_trace_and_stop(__func__);
}


acpi_status acpi_buffer_to_resource(u8 * aml_buffer,u16 aml_buffer_length,
                                    struct acpi_resource ** resource_ptr)
{
	lx_emul_trace_and_stop(__func__);
}


acpi_status acpi_install_address_space_handler(acpi_handle device,
                                               acpi_adr_space_type space_id,
                                               acpi_adr_space_handler handler,
                                               acpi_adr_space_setup setup,
                                               void * context)
{
	lx_emul_trace_and_stop(__func__);
}


int acpi_video_register(void)
{
	lx_emul_trace(__func__);
	return 0;
}


acpi_status acpi_get_name(acpi_handle object, u32 name_type,
                          struct acpi_buffer *ret_path_ptr)
{
	lx_emul_trace_and_stop(__func__);
}


acpi_status acpi_get_table(acpi_string signature, u32 instance, struct acpi_table_header **out_table)
{
	lx_emul_trace_and_stop(__func__);
}


void acpi_put_table(struct acpi_table_header *table)
{
	lx_emul_trace_and_stop(__func__);
}


void cpu_latency_qos_add_request(struct pm_qos_request *req, s32 value)
{
	lx_emul_trace(__func__);
}


void cpu_latency_qos_update_request(struct pm_qos_request *req, s32 new_value)
{
	lx_emul_trace(__func__);
}


bool cpu_latency_qos_request_active(struct pm_qos_request *req)
{
	lx_emul_trace_and_stop(__func__);
}


void cpu_latency_qos_remove_request(struct pm_qos_request *req)
{
	lx_emul_trace_and_stop(__func__);
}


enum acpi_backlight_type acpi_video_get_backlight_type(void)
{
	lx_emul_trace_and_stop(__func__);
}


bool pciehp_is_native(struct pci_dev *bridge)
{
	lx_emul_trace_and_stop(__func__);
}


struct irq_domain *pci_host_bridge_acpi_msi_domain(struct pci_bus *bus)
{
	lx_emul_trace(__func__);
	return NULL;
}


int pci_acpi_program_hp_params(struct pci_dev *dev)
{
	lx_emul_trace(__func__);
	return 0;
}


int pnp_range_reserved(resource_size_t start, resource_size_t end)
{
	lx_emul_trace_and_stop(__func__);
}


int utf16s_to_utf8s(const wchar_t *pwcs, int len, enum utf16_endian endian,
                    u8 *s, int maxlen)
{
	lx_emul_trace_and_stop(__func__);
}


int __get_user_nocheck_1(void)
{
	lx_emul_trace_and_stop(__func__);
}


unsigned long
raw_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


unsigned long
raw_copy_from_user(void *to, const void __user * from, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


int i915_gemfs_init(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
	return 0;
}


int __init i915_global_context_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


int __init i915_global_gem_context_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


int __init i915_global_request_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


int __init i915_global_scheduler_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


void pci_fixup_device(enum pci_fixup_pass pass,struct pci_dev * dev)
{
	lx_emul_trace(__func__);
}


int pci_dev_specific_reset(struct pci_dev * dev,int probe)
{
	lx_emul_trace(__func__);
	return 0;
}



int register_acpi_notifier(struct notifier_block * nb)
{
	lx_emul_trace(__func__);
	return 0;
}


int unregister_acpi_notifier(struct notifier_block * nb)
{
	lx_emul_trace(__func__);
	return 0;
}


int acpi_reconfig_notifier_register(struct notifier_block * nb)
{
	lx_emul_trace(__func__);
	return 0;
}


int intel_vgt_balloon(struct i915_ggtt * ggtt)
{
	lx_emul_trace(__func__);
	return 0;
}


void intel_gt_info_print(const struct intel_gt_info * info,struct drm_printer * p)
{
	lx_emul_trace(__func__);
}


int set_pages_uc(struct page * page,int numpages)
{
	lx_emul_trace(__func__);
	return 0;
}


bool irq_work_queue(struct irq_work * work)
{
	lx_emul_trace(__func__);
	return false;
}


void iomap_free(resource_size_t base, unsigned long size)
{
	lx_emul_trace_and_stop(__func__);
}


void i915_gem_object_release_mmap_offset(struct drm_i915_gem_object * obj)
{
	lx_emul_trace(__func__);
}


#include <net/net_namespace.h>

struct net init_net;


#include <linux/skbuff.h>

void kfree_skb(struct sk_buff * skb)
{
	if (!skb)
		return;

	lx_emul_trace(__func__);
	printk("%s:%d: leaking skb: %p\n", __func__, __LINE__, skb);
}


#include <net/net_namespace.h>

int register_pernet_subsys(struct pernet_operations * ops)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/firmware.h>

void release_firmware(const struct firmware * fw)
{
	lx_emul_trace(__func__);
}


#include <linux/firmware.h>

int request_firmware(const struct firmware ** firmware_p,const char * name,struct device * device)
{
	lx_emul_trace(__func__);
	return -1;
}


#include <linux/firmware.h>

int request_firmware_direct(const struct firmware ** firmware_p,const char * name,struct device * device)
{
	lx_emul_trace(__func__);
	return -1;
}


int ___ratelimit(struct ratelimit_state * rs, const char * func)
{
	/*
	 * from lib/ratelimit.c:
	 * " 0 means callbacks will be suppressed.
	 *   1 means go ahead and do it. "
	 */
	lx_emul_trace(__func__);
	return 1;
}


void intel_dsb_prepare(struct intel_crtc_state *crtc_state)
{
	lx_emul_trace(__func__);
}


void intel_dsb_cleanup(struct intel_crtc_state *crtc_state)
{
	lx_emul_trace(__func__);
}



void intel_dsb_commit(const struct intel_crtc_state *crtc_state)
{
	lx_emul_trace(__func__);
}
